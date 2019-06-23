package main

import (
	"context"
	"errors"
	"fmt"
	"io"
	"net/http"
	"os"
	"regexp"
	"time"

	"cloud.google.com/go/storage"
	"github.com/julienschmidt/httprouter"
)

const apiKey = "c868a5215b6bfb6161c6a43363e62d45"

var nameRe = regexp.MustCompile(`^[A_Za-z0-9-]+$`)

type solverType string

const (
	puzzleSolver solverType = "puzzle"
	taskSolver              = "task"
)

type handler struct {
	cl *storage.Client
}

func newHandler(cl *storage.Client) http.Handler {
	h := &handler{cl: cl}
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

		path, err := h.stageEphemeralPackage(ctx, w, name, f)
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
	for i := 1; i <= 10; i++ {
		fmt.Fprintf(w, "%d...\n", i)
		time.Sleep(time.Second)
	}
	return errors.New("not implemented")
}

func (h *handler) stageEphemeralPackage(ctx context.Context, w io.Writer, name string, f io.Reader) (string, error) {
	return "", errors.New("not implemented")
}

func (h *handler) verifyPackage(ctx context.Context, w io.Writer, path string, typ solverType) error {
	return errors.New("not implemented")
}

func (h *handler) stageProductionPackage(ctx context.Context, w io.Writer, name string, typ solverType, path string) error {
	return errors.New("not implemented")
}
