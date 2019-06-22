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

type args struct {
	block           int
	apiKey          string
	slackWebhookURL string
	dryRun          bool
}

func parseArgs() (*args, error) {
	var args args
	fs := flag.NewFlagSet("", flag.ContinueOnError)
	fs.IntVar(&args.block, "block", -1, "block number")
	fs.StringVar(&args.apiKey, "apikey", "", "exec API key")
	fs.StringVar(&args.slackWebhookURL, "slackwebhookurl", "", "Slack webhook URL to log")
	fs.BoolVar(&args.dryRun, "dryrun", false, "dry-run")
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

		cl, err := storage.NewClient(ctx)
		if err != nil {
			return err
		}

		m := &miner{
			args: args,
			l:    newLogger(args.slackWebhookURL),
			cl:   cl,
		}
		defer m.l.Close()
		if err := m.Run(ctx); err != nil {
			m.l.Logf("@everyone ERROR: %v", err)
			return err
		}
		m.l.Log("Success!")
		return nil
	}(); err != nil {
		panic(fmt.Sprint("ERROR: ", err))
	}
}

type miner struct {
	args *args
	l    *logger
	cl   *storage.Client
}

func (m *miner) Run(ctx context.Context) error {
	m.l.Logf("Start mining block %d", m.args.block)

	// TODO: Check timestamp.
	runCtx, cancel := context.WithTimeout(ctx, 10*time.Minute)
	defer cancel()

	puzzleCh := make(chan string, 1)
	taskCh := make(chan string, 1)
	go func() {
		name, puzzle, err := m.runPuzzleSolvers(runCtx)
		if err != nil {
			m.l.Logf("Warning: Failed to run puzzle solvers: %v", err)
		} else {
			m.l.Logf("Selected the puzzle solution by %s", name)
		}
		puzzleCh <- puzzle
	}()
	go func() {
		name, task, err := m.runTaskSolvers(runCtx)
		if err != nil {
			m.l.Logf("Warning: Failed to run task solvers: %v", err)
		} else {
			m.l.Logf("Selected the task solution by %s", name)
		}
		taskCh <- task
	}()

	puzzle := <-puzzleCh
	task := <-taskCh

	if m.args.dryRun {
		return nil
	}

	if puzzle == "" || task == "" {
		return errors.New("no valid solution pair to the block problem")
	}

	m.l.Log("Submitting...")
	return m.submit(ctx, puzzle, task)
}

func (m *miner) queryPackages(ctx context.Context, prefix string) ([]string, error) {
	h := m.cl.Bucket(bucket).Objects(ctx, &storage.Query{
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

func (m *miner) runPuzzleSolvers(ctx context.Context) (name, solution string, _ error) {
	ctx, cancel := context.WithCancel(ctx)
	defer cancel()

	names, err := m.queryPackages(ctx, "packages/solvers/puzzle/")
	if err != nil {
		return "", "", nil
	}

	type result struct {
		name     string
		solution string
		err      error
	}

	ch := make(chan *result)

	for _, name := range names {
		name := name
		go func() {
			solution, err := m.runPuzzleSolver(ctx, name)
			if err != nil && ctx.Err() == nil {
				// Retry automatically since puzzle solutions are so important.
				solution, err = m.runPuzzleSolver(ctx, name)
			}
			ch <- &result{name, solution, err}
		}()
	}

	done := 0
	for done < len(names) {
		select {
		case r := <-ch:
			if r.err == nil {
				m.l.Logf("Puzzle solver %s passed: https://storage.googleapis.com/%s/results/%d/solvers/puzzle/%s/out.txt", r.name, bucket, m.args.block, r.name)
				return r.name, r.solution, nil
			}
			m.l.Logf("Warning: Failed to run the puzzle solver %s: %v", r.name, r.err)
			done++
		case <-ctx.Done():
			return "", "", fmt.Errorf("no puzzle solver passed before deadline: %v", ctx.Err())
		}
	}
	return "", "", errors.New("all puzzle solver failed")
}

func (m *miner) runPuzzleSolver(ctx context.Context, name string) (string, error) {
	t := &task{
		Cmd: `set -e
ls -l
./solve-puzzle < puzzle.cond > $OUT_DIR/out.txt 2> /dev/null
curl -s -d "task=$(cat puzzle.cond)" -d "solution=$(cat $OUT_DIR/out.txt)" https://us-central1-sound-type-system.cloudfunctions.net/checkpuzzle > $OUT_DIR/validation.txt
grep -q Success $OUT_DIR/validation.txt
`,
		Pkgs: []*pkg{
			{URL: fmt.Sprintf("gs://%s/blocks/%d/block.tar.gz", bucket, m.args.block)},
			{URL: fmt.Sprintf("gs://%s/packages/solvers/puzzle/%s.tar.gz", bucket, name)},
		},
		Out: fmt.Sprintf("gs://%s/results/%d/solvers/puzzle/%s/", bucket, m.args.block, name),
	}
	m.l.Logf("Running the puzzle solver %s", name)
	if err := m.runTask(ctx, t); err != nil {
		return "", err
	}
	b, err := m.readGCSFile(ctx, fmt.Sprintf("results/%d/solvers/puzzle/%s/out.txt", m.args.block, name))
	if err != nil {
		return "", err
	}
	return strings.TrimSpace(string(b)), nil
}

func (m *miner) runTaskSolvers(ctx context.Context) (name, solution string, _ error) {
	ctx, cancel := context.WithCancel(ctx)
	defer cancel()

	names, err := m.queryPackages(ctx, "packages/solvers/task/")
	if err != nil {
		return "", "", nil
	}

	type result struct {
		name     string
		solution string
		score    int
		err      error
	}

	ch := make(chan *result)

	for _, name := range names {
		name := name
		go func() {
			sol, score, err := m.runTaskSolver(ctx, name)
			ch <- &result{name, sol, score, err}
		}()
	}

	var best *result
	done := 0
loop:
	for done < len(names) {
		select {
		case r := <-ch:
			if r.err != nil {
				m.l.Logf("Warning: Failed to run the task solver %s: %v", r.name, r.err)
			} else {
				m.l.Logf("Task solver %s passed with score %d: https://storage.googleapis.com/%s/results/%d/solvers/puzzle/%s/out.txt", r.name, r.score, bucket, m.args.block, r.name)
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
		return "", "", errors.New("no task solver passed")
	}
	return best.name, best.solution, nil
}

func (m *miner) runTaskSolver(ctx context.Context, name string) (string, int, error) {
	t := &task{
		Cmd: `set -e
ls -l
./solve-task < task.desc > $OUT_DIR/out.txt 2> /dev/null
curl -s -d "task=$(cat task.desc)" -d "solution=$(cat $OUT_DIR/out.txt)" https://us-central1-sound-type-system.cloudfunctions.net/checktask > $OUT_DIR/validation.txt
grep -q Success $OUT_DIR/validation.txt
`,
		Pkgs: []*pkg{
			{URL: fmt.Sprintf("gs://%s/blocks/%d/block.tar.gz", bucket, m.args.block)},
			{URL: fmt.Sprintf("gs://%s/packages/solvers/task/%s.tar.gz", bucket, name)},
		},
		Out: fmt.Sprintf("gs://%s/results/%d/solvers/task/%s/", bucket, m.args.block, name),
	}
	m.l.Logf("Running the task solver %s", name)
	if err := m.runTask(ctx, t); err != nil {
		return "", 0, err
	}
	b, err := m.readGCSFile(ctx, fmt.Sprintf("results/%d/solvers/task/%s/out.txt", m.args.block, name))
	if err != nil {
		return "", 0, err
	}
	sol := strings.TrimSpace(string(b))
	b, err = m.readGCSFile(ctx, fmt.Sprintf("results/%d/solvers/task/%s/validation.txt", m.args.block, name))
	if err != nil {
		return "", 0, err
	}
	ms := regexp.MustCompile(`Your solution took (\d+) time units`).FindStringSubmatch(string(b))
	if ms == nil {
		return "", 0, errors.New("validator output does not contain steps")
	}
	score, err := strconv.Atoi(ms[1])
	if err != nil {
		return "", 0, err
	}
	return sol, score, nil
}

func (m *miner) runTask(ctx context.Context, t *task) error {
	body, err := json.Marshal(t)
	if err != nil {
		return err
	}

	req, err := http.NewRequest(http.MethodPost, "https://tasklet-r336oz7mza-uc.a.run.app/exec", bytes.NewReader(body))
	if err != nil {
		return err
	}
	req.Header.Set("X-API-Key", m.args.apiKey)
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

func (m *miner) readGCSFile(ctx context.Context, path string) ([]byte, error) {
	r, err := m.cl.Bucket(bucket).Object(path).NewReader(ctx)
	if err != nil {
		return nil, err
	}
	defer r.Close()
	return ioutil.ReadAll(r)
}

func (m *miner) submit(ctx context.Context, puzzle, task string) error {
	blockDir := filepath.Join("blocks", strconv.Itoa(m.args.block))
	if err := ioutil.WriteFile(filepath.Join(blockDir, "submit.desc"), []byte(puzzle), 0666); err != nil {
		return err
	}
	if err := ioutil.WriteFile(filepath.Join(blockDir, "submit.sol"), []byte(task), 0666); err != nil {
		return err
	}

	cmd := exec.CommandContext(ctx,
		"python3.6",
		"./lambda-cli.py",
		"submit",
		strconv.Itoa(m.args.block),
		filepath.Join(blockDir, "submit.sol"),
		filepath.Join(blockDir, "submit.desc"))
	out, err := cmd.CombinedOutput()
	m.l.Log(string(out))
	return err
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

type logger struct {
	msgs            chan string
	done            chan struct{}
	slackWebhookURL string
}

func newLogger(slackWebhookURL string) *logger {
	l := &logger{
		msgs:            make(chan string, 1000),
		done:            make(chan struct{}),
		slackWebhookURL: slackWebhookURL,
	}
	go l.run()
	return l
}

func (l *logger) Close() {
	close(l.msgs)
	<-l.done
}

func (l *logger) Log(args ...interface{}) {
	msg := fmt.Sprint(args...)
	l.msgs <- msg
}

func (l *logger) Logf(format string, args ...interface{}) {
	msg := fmt.Sprintf(format, args...)
	l.msgs <- msg
}

func (l *logger) run() {
	defer close(l.done)
	for msg := range l.msgs {
		l.emit(msg)
	}
}

func (l *logger) emit(msg string) error {
	fmt.Fprintln(os.Stderr, msg)

	if l.slackWebhookURL == "" {
		return nil
	}

	req := map[string]string{"text": msg}
	data, err := json.Marshal(req)
	if err != nil {
		return err
	}
	_, err = http.Post(l.slackWebhookURL, "application/json", bytes.NewReader(data))
	return err
}
