package main

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io/ioutil"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"time"

	"cloud.google.com/go/storage"
	_ "github.com/go-sql-driver/mysql"
	"google.golang.org/api/iterator"
)

const bucket = "sound-type-system"

var apiKey string

type args struct {
	block  int
	apiKey string
	submit bool
}

func parseArgs() (*args, error) {
	var args args
	fs := flag.NewFlagSet("", flag.ContinueOnError)
	fs.IntVar(&args.block, "block", -1, "block number")
	fs.StringVar(&args.apiKey, "apikey", "", "exec API key")
	fs.BoolVar(&args.submit, "submit", true, "submit")
	return &args, fs.Parse(os.Args[1:])
}

func main() {
	ctx := context.Background()

	if err := func() error {
		args, err := parseArgs()
		if err != nil {
			return err
		}

		if args.block < 0 {
			return errors.New("-block required")
		}

		if args.apiKey == "" {
			return errors.New("-apikey required")
		}

		apiKey = args.apiKey

		cl, err := storage.NewClient(ctx)
		if err != nil {
			return err
		}

		return doMain(ctx, cl, args.block, args.submit)
	}(); err != nil {
		panic(fmt.Sprint("ERROR: ", err))
	}
}

func doMain(ctx context.Context, cl *storage.Client, block int, toSubmit bool) error {
	puzzlers, err := queryPackages(ctx, cl, "packages/puzzlers/")
	if err != nil {
		return err
	}

	taskers, err := queryPackages(ctx, cl, "packages/taskers/")
	if err != nil {
		return err
	}

	// TODO: Check timestamp.
	runCtx, cancel := context.WithTimeout(ctx, 10*time.Minute)
	defer cancel()

	puzzleCh := make(chan string, 1)
	taskCh := make(chan string, 1)
	go func() {
		puzzle, err := runPuzzlers(runCtx, cl, puzzlers, block)
		if err != nil {
			fmt.Fprintf(os.Stderr, "ERROR: Failed to run puzzler: %v\n", err)
		}
		puzzleCh <- puzzle
	}()
	go func() {
		task, err := runTaskers(runCtx, cl, taskers, block)
		if err != nil {
			fmt.Fprintf(os.Stderr, "ERROR: Failed to run tasker: %v\n", err)
		}
		taskCh <- task
	}()

	puzzle := <-puzzleCh
	task := <-taskCh

	fmt.Fprintf(os.Stderr, "Submitting:\nPuzzle=%s\nTask=%s\n", puzzle, task)
	if !toSubmit {
		return nil
	}

	if puzzle == "" || task == "" {
		return errors.New("not submitting")
	}

	return submit(ctx, block, puzzle, task)
}

func queryPackages(ctx context.Context, cl *storage.Client, prefix string) ([]string, error) {
	h := cl.Bucket(bucket).Objects(ctx, &storage.Query{
		Prefix: prefix,
	})
	var names []string
	for {
		a, err := h.Next()
		if err == iterator.Done {
			break
		} else if err != nil {
			return nil, err
		}
		name := strings.Split(strings.TrimPrefix(a.Name, prefix), ".")[0]
		names = append(names, name)
	}
	return names, nil
}

func runPuzzlers(ctx context.Context, cl *storage.Client, puzzlers []string, block int) (string, error) {
	ctx, cancel := context.WithCancel(ctx)
	defer cancel()

	type result struct {
		name     string
		solution string
		err      error
	}

	ch := make(chan *result)

	for _, name := range puzzlers {
		name := name
		go func() {
			sol, err := runPuzzler(ctx, cl, block, name)
			ch <- &result{name, sol, err}
		}()
	}

	done := 0
	for done < len(puzzlers) {
		select {
		case r := <-ch:
			if r.err == nil {
				fmt.Fprintf(os.Stderr, "Puzzler %s passed\n", r.name)
				return r.solution, nil
			}
			fmt.Fprintf(os.Stderr, "ERROR: Failed to run puzzler %s: %v\n", r.name, r.err)
			done++
		case <-ctx.Done():
			return "", fmt.Errorf("no puzzler passed before deadline: %v", ctx.Err())
		}
	}
	return "", errors.New("all puzzler failed")
}

func runPuzzler(ctx context.Context, cl *storage.Client, block int, name string) (string, error) {
	t := &task{
		Cmd: `set -e
ls -l
./solve-puzzle < puzzle.cond > $OUT_DIR/out.txt 2> /dev/null
curl -s -d "task=$(cat puzzle.cond)" -d "solution=$(cat $OUT_DIR/out.txt)" https://us-central1-sound-type-system.cloudfunctions.net/checkpuzzle > $OUT_DIR/validation.txt
grep -q Success $OUT_DIR/validation.txt
`,
		Pkgs: []*pkg{
			{URL: fmt.Sprintf("gs://%s/blocks/%d/block.tar.gz", bucket, block)},
			{URL: fmt.Sprintf("gs://%s/packages/puzzlers/%s.tar.gz", bucket, name)},
		},
		Out: fmt.Sprintf("gs://%s/results/%d/puzzlers/%s/", bucket, block, name),
	}
	fmt.Fprintf(os.Stderr, "Running puzzler %s\n", name)
	if err := runTask(ctx, t); err != nil {
		return "", err
	}
	b, err := readGCSFile(ctx, cl, fmt.Sprintf("results/%d/puzzlers/%s/out.txt", block, name))
	if err != nil {
		return "", err
	}
	return strings.TrimSpace(string(b)), nil
}

func runTaskers(ctx context.Context, cl *storage.Client, taskers []string, block int) (string, error) {
	ctx, cancel := context.WithCancel(ctx)
	defer cancel()

	type result struct {
		name     string
		solution string
		score    int
		err      error
	}

	ch := make(chan *result)

	for _, name := range taskers {
		name := name
		go func() {
			sol, score, err := runTasker(ctx, cl, block, name)
			ch <- &result{name, sol, score, err}
		}()
	}

	var best *result
	done := 0
loop:
	for done < len(taskers) {
		select {
		case r := <-ch:
			if r.err != nil {
				fmt.Fprintf(os.Stderr, "ERROR: Failed to run tasker %s: %v\n", r.name, r.err)
			} else {
				fmt.Fprintf(os.Stderr, "Tasker %s passed with score %d\n", r.name, r.score)
				if best == nil || r.score < best.score {
					best = r
				}
			}
			done++
		case <-ctx.Done():
			break loop
		}
	}

	if best == nil {
		return "", errors.New("no tasker passed")
	}
	return best.solution, nil
}

func runTasker(ctx context.Context, cl *storage.Client, block int, name string) (string, int, error) {
	t := &task{
		Cmd: `set -e
ls -l
./solve-task < task.desc > $OUT_DIR/out.txt 2> /dev/null
curl -s -d "task=$(cat task.desc)" -d "solution=$(cat $OUT_DIR/out.txt)" https://us-central1-sound-type-system.cloudfunctions.net/checktask > $OUT_DIR/validation.txt
grep -q Success $OUT_DIR/validation.txt
`,
		Pkgs: []*pkg{
			{URL: fmt.Sprintf("gs://%s/blocks/%d/block.tar.gz", bucket, block)},
			{URL: fmt.Sprintf("gs://%s/packages/taskers/%s.tar.gz", bucket, name)},
		},
		Out: fmt.Sprintf("gs://%s/results/%d/taskers/%s/", bucket, block, name),
	}
	fmt.Fprintf(os.Stderr, "Running tasker %s\n", name)
	if err := runTask(ctx, t); err != nil {
		return "", 0, err
	}
	b, err := readGCSFile(ctx, cl, fmt.Sprintf("results/%d/taskers/%s/out.txt", block, name))
	if err != nil {
		return "", 0, err
	}
	sol := strings.TrimSpace(string(b))
	b, err = readGCSFile(ctx, cl, fmt.Sprintf("results/%d/taskers/%s/validation.txt", block, name))
	if err != nil {
		return "", 0, err
	}
	m := regexp.MustCompile(`Your solution took (\d+) time units`).FindStringSubmatch(string(b))
	if m == nil {
		return "", 0, errors.New("validator output does not contain steps")
	}
	score, err := strconv.Atoi(m[1])
	if err != nil {
		return "", 0, err
	}
	return sol, score, nil
}

func runTask(ctx context.Context, t *task) error {
	body, err := json.Marshal(t)
	if err != nil {
		return err
	}

	req, err := http.NewRequest(http.MethodPost, "https://tasklet-r336oz7mza-uc.a.run.app/exec", bytes.NewReader(body))
	if err != nil {
		return err
	}
	req.Header.Set("X-API-Key", apiKey)
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

func readGCSFile(ctx context.Context, cl *storage.Client, path string) ([]byte, error) {
	r, err := cl.Bucket(bucket).Object(path).NewReader(ctx)
	if err != nil {
		return nil, err
	}
	defer r.Close()
	return ioutil.ReadAll(r)
}

func submit(ctx context.Context, block int, puzzle, task string) error {
	blockDir := filepath.Join("blocks", strconv.Itoa(block))
	if err := ioutil.WriteFile(filepath.Join(blockDir, "submit.desc"), []byte(puzzle), 0666); err != nil {
		return err
	}
	if err := ioutil.WriteFile(filepath.Join(blockDir, "submit.sol"), []byte(task), 0666); err != nil {
		return err
	}
	f, err := os.Create(filepath.Join(blockDir, "submit.log"))
	if err != nil {
		return err
	}
	defer f.Close()
	cmd := exec.CommandContext(ctx,
		"python3.6",
		"./lambda-cli.py",
		"submit",
		strconv.Itoa(block),
		filepath.Join(blockDir, "submit.sol"),
		filepath.Join(blockDir, "submit.desc"))
	cmd.Stdout = f
	cmd.Stderr = f
	return cmd.Run()
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
