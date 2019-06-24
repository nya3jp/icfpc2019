package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"io/ioutil"
	"net/http"
	"os"
	"strings"
	"time"

	"cloud.google.com/go/storage"

	"github.com/nya3jp/icfpc2019/go/infra/concurrent"
	"github.com/nya3jp/icfpc2019/go/infra/tasklet"
)

type args struct {
	solver   string
	problems []string
	purchase string
	timeout  time.Duration
	execKey  string
}

func parseArgs() (*args, error) {
	var args args
	var timeout int
	fs := flag.NewFlagSet("", flag.ContinueOnError)
	fs.StringVar(&args.solver, "solver", "", "")
	fs.StringVar(&args.purchase, "purchase", "default", "")
	fs.IntVar(&timeout, "timeout", 0, "")
	fs.StringVar(&args.execKey, "execkey", "", "")

	if err := fs.Parse(os.Args[1:]); err != nil {
		return nil, err
	}

	args.timeout = time.Duration(timeout) * time.Second
	args.problems = fs.Args()

	if args.solver == "" {
		return nil, errors.New("-solver must be specified")
	}
	if args.purchase == "default" {
		return nil, errors.New("-purchase must be specified")
	}
	if timeout == 0 {
		return nil, errors.New("-timeout must be specified")
	}
	if err := validatePurchase(args.purchase); err != nil {
		return nil, err
	}
	if args.execKey == "" {
		return nil, errors.New("-execkey must be specified")
	}
	if len(args.problems) == 0 {
		return nil, errors.New("problems must be specified")
	}
	return &args, nil
}

var validBoosters = map[rune]struct{}{
	'B': {},
	'F': {},
	'L': {},
	'R': {},
	'C': {},
}

func validatePurchase(purchase string) error {
	for _, c := range purchase {
		if _, ok := validBoosters[c]; !ok {
			return fmt.Errorf("invalid purchase %c", c)
		}
	}
	for i := 0; i+1 < len(purchase); i++ {
		a, b := purchase[i], purchase[i+1]
		if a > b {
			return errors.New("purchase must be sorted")
		}
	}
	return nil
}

func main() {
	if err := doMain(); err != nil {
		panic(fmt.Sprint("ERROR: ", err))
	}
}

func doMain() error {
	ctx := context.Background()

	args, err := parseArgs()
	if err != nil {
		return err
	}

	sc, err := storage.NewClient(ctx)
	if err != nil {
		return err
	}

	tc := tasklet.NewClient(http.DefaultClient, args.execKey)

	logf("Running %d jobs...", len(args.problems))

	ch := make(chan time.Duration, len(args.problems))
	lim := concurrent.NewLimit(300)
	go func() {
		for _, problem := range args.problems {
			problem := problem
			go func() {
				defer lim.Use(1)()
				runtime, err := evaluate(ctx, sc, tc, problem, args.purchase, args.solver, args.timeout)
				if err != nil {
					logf("ERROR: %s: %v", problem, err)
				}
				ch <- runtime
			}()
			time.Sleep(100 * time.Millisecond)
		}
	}()

	var total time.Duration
	for i := range args.problems {
		fmt.Fprintf(os.Stderr, "%d/%d done (￥%.1f)\r", i, len(args.problems), price(total))
		t := <-ch
		total += t
	}

	logf("Finished! Total %v (￥%.1f)", total.Round(time.Second), price(total))
	return nil
}

func evaluate(ctx context.Context, sc *storage.Client, tc *tasklet.Client, problem, purchase, solver string, timeout time.Duration) (runtime time.Duration, retErr error) {
	problemPlus := problem
	if purchase != "" {
		problemPlus += "_" + purchase
	}

	defer func() {
		if retErr != nil {
			retErr = fmt.Errorf("%v; check logs at gs://sound-type-system/evals/%s/%s/", retErr, solver, problemPlus)
		}
	}()

	start := time.Now()

	task := &tasklet.Task{
		Cmd: fmt.Sprintf(`set -ex
cp problems/%s.desc task.desc
timeout %f ./solve-task %s < task.desc > $OUT_DIR/out.txt 2> /dev/null
while :; do
  curl -s -d '{"solver": "%s", "problem": "%s", "purchase": "%s", "solution": "'"$(cat "$OUT_DIR/out.txt")"'"}' -u :c868a5215b6bfb6161c6a43363e62d45 http://sound.nya3.jp/submit | tee $OUT_DIR/validation.txt
  if ! grep -q "The server encountered an error" $OUT_DIR/validation.txt; then break; fi
  sleep $(python3 -c 'import random; print(random.random()*10)')
done
if grep -q "The server encountered an error" $OUT_DIR/validation.txt; then exit 50; fi
if grep -q "duplicate solution" $OUT_DIR/validation.txt; then exit 0; fi
if grep -q ERROR $OUT_DIR/validation.txt; then exit 40; fi
`, problem, timeout.Seconds(), purchase, solver, problem, purchase),
		Pkgs: []*tasklet.Pkg{
			{URL: "gs://sound-type-system/packages/problems.tar.gz"},
			{URL: fmt.Sprintf("gs://sound-type-system/packages/solvers/task/%s.tar.gz", solver)},
		},
		Out: fmt.Sprintf("gs://sound-type-system/evals/%s/%s/", solver, problemPlus),
	}
	err := tc.Run(ctx, task)
	runtime = time.Since(start)
	if err != nil {
		return runtime, err
	}

	obj := sc.Bucket("sound-type-system").Object(fmt.Sprintf("evals/%s/%s/validation.txt", solver, problemPlus))
	r, err := obj.NewReader(ctx)
	if err != nil {
		return runtime, err
	}
	defer r.Close()

	b, err := ioutil.ReadAll(r)
	if err != nil {
		return runtime, err
	}

	logf("PASS: %s (%v): %s", problem, runtime.Round(time.Second), strings.TrimSpace(string(b)))
	return runtime, nil
}

func log(args ...interface{}) {
	fmt.Fprintf(os.Stderr, "%s\n", fmt.Sprint(args...))
}

func logf(format string, args ...interface{}) {
	fmt.Fprintf(os.Stderr, "%s\n", fmt.Sprintf(format, args...))
}

func price(t time.Duration) float64 {
	return t.Seconds() * 0.000029 * 107.32
}
