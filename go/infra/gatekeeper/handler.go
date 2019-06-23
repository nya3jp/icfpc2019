package main

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"io/ioutil"
	"net/http"
	"os"
	"regexp"
	"time"

	"cloud.google.com/go/storage"
	"github.com/julienschmidt/httprouter"
)

const (
	apiKey          = "c868a5215b6bfb6161c6a43363e62d45"
	bucket          = "sound-type-system"
	exampleBlockURL = "gs://sound-type-system/blocks/example-v1.tar.gz"
)

var nameRe = regexp.MustCompile(`^[A_Za-z0-9-]+$`)

type solverType string

const (
	puzzleSolver solverType = "puzzle"
	taskSolver              = "task"
)

type handler struct {
	cl      *storage.Client
	execKey string
}

func newHandler(cl *storage.Client, execKey string) http.Handler {
	h := &handler{cl: cl, execKey: execKey}
	r := httprouter.New()
	r.Handle("POST", "/gatekeeper/add", h.handleAdd)
	return r
}

func handle(w http.ResponseWriter, r *http.Request, f func(context.Context) error) {
	ctx := r.Context()
	if _, pass, ok := r.BasicAuth(); !ok || pass != apiKey {
		w.Header().Set("WWW-Authenticate", `Basic realm="Need valid API key"`)
		http.Error(w, "Need valid API key", http.StatusUnauthorized)
		return
	}
	if err := f(ctx); err != nil {
		msg := fmt.Sprintf("ERROR: %v", err)
		fmt.Fprintln(os.Stderr, msg)
		fmt.Fprintln(w, msg)
	}
}

type flushableWriter interface {
	io.Writer
	http.Flusher
}

type flushingWriter struct {
	w flushableWriter
}

func (w *flushingWriter) Write(p []byte) (int, error) {
	defer w.w.Flush()
	return w.w.Write(p)
}

func (h *handler) handleAdd(rw http.ResponseWriter, r *http.Request, _ httprouter.Params) {
	handle(rw, r, func(ctx context.Context) error {
		rw.Header().Set("Content-Type", "text/plain")
		rw.WriteHeader(200)

		var w io.Writer = &flushingWriter{rw.(flushableWriter)}

		if err := r.ParseMultipartForm(16 * 1024 * 1024); err != nil {
			return err
		}

		typs := r.MultipartForm.Value["type"]
		if len(typs) != 1 {
			return errors.New("type parameter not found")
		}
		var typ solverType
		switch t := solverType(typs[0]); t {
		case puzzleSolver, taskSolver:
			typ = t
		default:
			return errors.New("unknown solver type")
		}

		names := r.MultipartForm.Value["name"]
		if len(names) != 1 {
			return errors.New("name parameter not found")
		}
		name := names[0]
		if !nameRe.MatchString(name) {
			return errors.New("invalid name")
		}

		f, _, err := r.FormFile("pkg")
		if err != nil {
			return err
		}
		defer f.Close()

		if err := h.checkPackage(ctx, w, name, typ); err != nil {
			return err
		}

		path, err := h.stageEphemeralPackage(ctx, w, name, typ, f)
		if err != nil {
			return err
		}

		if err := h.verifyPackage(ctx, w, path, typ); err != nil {
			return err
		}

		if err := h.stageProductionPackage(ctx, w, name, typ, path); err != nil {
			return err
		}

		fmt.Fprintln(w, "OK!")
		return nil
	})
}

func (h *handler) checkPackage(ctx context.Context, w io.Writer, name string, typ solverType) error {
	obj := h.cl.Bucket(bucket).Object(pkgPath(name, typ))
	if _, err := obj.Attrs(ctx); err == nil {
		return fmt.Errorf("package for %s solver %s already exists; choose another name", typ, name)
	}
	return nil
}

func (h *handler) stageEphemeralPackage(ctx context.Context, w io.Writer, name string, typ solverType, f io.Reader) (string, error) {
	ctx, cancel := context.WithCancel(ctx)
	defer cancel()

	fmt.Fprintln(w, "Staging an ephemeral package...")

	path := fmt.Sprintf("packages/staging/solver-%s-%s.%s.tar.gz", typ, name, time.Now().Format("20060102-150405"))
	d := h.cl.Bucket(bucket).Object(path).NewWriter(ctx)
	if _, err := io.Copy(d, f); err != nil {
		cancel()
		d.Close()
		return "", err
	}
	if err := d.Close(); err != nil {
		return "", err
	}
	return path, nil
}

func (h *handler) verifyPackage(ctx context.Context, w io.Writer, path string, typ solverType) error {
	outPrefix := fmt.Sprintf("results/staging/%s/", time.Now().Format("20060102-150405"))
	var cmd string
	switch typ {
	case puzzleSolver:
		cmd = `set -e
ls -l
./solve-puzzle < puzzle.cond > $OUT_DIR/out.txt 2> /dev/null
curl -s -d "task=$(cat puzzle.cond)" -d "solution=$(cat $OUT_DIR/out.txt)" https://us-central1-sound-type-system.cloudfunctions.net/checkpuzzle > $OUT_DIR/validation.txt
grep -q Success $OUT_DIR/validation.txt
`
	case taskSolver:
		cmd = `set -e
ls -l
./solve-task < task.desc > $OUT_DIR/out.txt 2> /dev/null
curl -s -d "task=$(cat task.desc)" -d "solution=$(cat $OUT_DIR/out.txt)" https://us-central1-sound-type-system.cloudfunctions.net/checktask > $OUT_DIR/validation.txt
grep -q Success $OUT_DIR/validation.txt
`
	default:
		cmd = "false"
	}

	t := &task{
		Cmd: cmd,
		Pkgs: []*pkg{
			{URL: exampleBlockURL},
			{URL: fmt.Sprintf("gs://%s/%s", bucket, path)},
		},
		Out: fmt.Sprintf("gs://%s/%s", bucket, outPrefix),
	}
	fmt.Fprintf(w, "Running a verification task...\nLogs: %s\n", t.Out)
	if err := h.runTask(ctx, t); err != nil {
		return fmt.Errorf("validation failed: %v", err)
	}

	r, err := h.cl.Bucket(bucket).Object(outPrefix + "validation.txt").NewReader(ctx)
	if err != nil {
		return err
	}
	defer r.Close()
	b, err := ioutil.ReadAll(r)
	if err != nil {
		return err
	}
	fmt.Fprintln(w, string(b))
	return nil
}

func (h *handler) runTask(ctx context.Context, t *task) error {
	body, err := json.Marshal(t)
	if err != nil {
		return err
	}

	req, err := http.NewRequest(http.MethodPost, "https://tasklet-r336oz7mza-uc.a.run.app/exec", bytes.NewReader(body))
	if err != nil {
		return err
	}
	req.Header.Set("X-API-Key", h.execKey)
	res, err := http.DefaultClient.Do(req.WithContext(ctx))
	if err != nil {
		return err
	}
	defer res.Body.Close()

	if res.StatusCode != http.StatusOK {
		b, _ := ioutil.ReadAll(res.Body)
		return fmt.Errorf("exec returned %d: %s", res.StatusCode, string(b))
	}

	type result struct {
		Code int `json:"code"`
	}
	var r result
	if err := json.NewDecoder(res.Body).Decode(&r); err != nil {
		return err
	}
	if r.Code != 0 {
		return fmt.Errorf("task terminated with exit code %d", r.Code)
	}
	return nil
}

func (h *handler) stageProductionPackage(ctx context.Context, w io.Writer, name string, typ solverType, ephPath string) error {
	fmt.Fprintln(w, "Staging a production package...")

	ctx, cancel := context.WithCancel(ctx)
	defer cancel()

	src, err := h.cl.Bucket(bucket).Object(ephPath).NewReader(ctx)
	if err != nil {
		return err
	}
	defer src.Close()

	dst := h.cl.Bucket(bucket).Object(pkgPath(name, typ)).NewWriter(ctx)
	if _, err := io.Copy(dst, src); err != nil {
		cancel()
		dst.Close()
		return err
	}
	return dst.Close()
}

func pkgPath(name string, typ solverType) string {
	return fmt.Sprintf("packages/solvers/%s/%s.tar.gz", typ, name)
}

type task struct {
	Cmd  string `json:"cmd"`
	Pkgs []*pkg `json:"pkgs"`
	Out  string `json:"out"`
}

type pkg struct {
	URL  string `json:"url"`
	Dest string `json:"dest,omitempty"`
}
