package main

import (
	"archive/zip"
	"bytes"
	"context"
	"database/sql"
	"encoding/csv"
	"encoding/json"
	"errors"
	"fmt"
	"html/template"
	"io/ioutil"
	"math"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"syscall"
	"time"

	"cloud.google.com/go/storage"
	"github.com/julienschmidt/httprouter"
	"golang.org/x/sync/errgroup"

	"github.com/nya3jp/icfpc2019/go/infra/concurrent"
)

type handler struct {
	db *sql.DB
	cl *storage.Client
}

func newHandler(db *sql.DB, cl *storage.Client) http.Handler {
	h := &handler{db: db, cl: cl}
	r := httprouter.New()
	r.Handle("GET", "/dashboard", h.handleIndex)
	r.Handle("GET", "/dashboard/arena", h.handleArena)
	r.Handle("GET", "/dashboard/problem/:problem", h.handleProblem)
	//r.Handle("GET", "/dashboard/zip", h.handleZip)
	//r.Handle("GET", "/dashboard/csv", h.handleCSV)
	r.ServeFiles("/static/*filepath", http.Dir("infra/dashboard/static"))
	return r
}

func handle(w http.ResponseWriter, r *http.Request, f func(context.Context) error) {
	ctx := r.Context()
	if err := f(ctx); err != nil {
		msg := fmt.Sprintf("ERROR: %v", err)
		fmt.Fprintln(os.Stderr, msg)
		w.Header().Set("Content-Type", "text/plain")
		http.Error(w, msg, http.StatusInternalServerError)
	}
}

var jst *time.Location

func init() {
	var err error
	jst, err = time.LoadLocation("Asia/Tokyo")
	if err != nil {
		panic(err)
	}
}

func localTime(t time.Time) time.Time {
	return t.In(jst)
}

var templateFuncs = template.FuncMap{
	"localTime": localTime,
}

func parseTemplate(filenames ...string) (*template.Template, error) {
	t := template.New(filepath.Base(filenames[0]))
	t.Funcs(templateFuncs)
	for _, fn := range filenames {
		if _, err := t.ParseFiles(filepath.Join("infra/dashboard/templates", fn)); err != nil {
			return nil, err
		}
	}
	return t, nil
}

type solution struct {
	ID        int
	Solver    string
	Problem   string
	Purchase  string
	Size      Size
	Evaluator string
	Score     int32
	BaseScore int32
	Submitted time.Time
}

func (s *solution) RatioStr() string {
	return fmt.Sprintf("%.3f", float64(s.BaseScore)/float64(s.Score))
}

func (s *solution) Weight() int {
	return int(math.Ceil(1000 * math.Log2(float64(s.Size.X)*float64(s.Size.Y))))
}

var indexTmpl = template.Must(parseTemplate("base.html", "index.html"))

type indexValues struct {
	Purchase      string
	BestSolutions []*solution
	Purchases     []string
	Balance       int
}

func (h *handler) handleIndex(w http.ResponseWriter, r *http.Request, _ httprouter.Params) {
	handle(w, r, func(ctx context.Context) error {
		purchase := r.URL.Query().Get("purchase")

		ss, err := h.queryBestSolutions(ctx, purchase)
		if err != nil {
			return err
		}

		bss := ss
		if purchase != "" {
			bss, err = h.queryBestSolutions(ctx, "")
			if err != nil {
				return err
			}
		}
		smap := make(map[string]*solution)
		for _, s := range ss {
			smap[s.Problem] = s
		}
		for _, bs := range bss {
			if s, ok := smap[bs.Problem]; ok {
				s.BaseScore = bs.Score
			}
		}

		ps, err := h.queryPurchases(ctx)
		if err != nil {
			return err
		}

		v := &indexValues{
			Purchase:      purchase,
			BestSolutions: ss,
			Purchases:     ps,
			Balance:       loadBalance(),
		}
		return indexTmpl.Execute(w, v)
	})
}

var arenaTmpl = template.Must(parseTemplate("base.html", "arena.html"))

type arenaValues struct {
	Purchase  string
	Problems  []string
	Solvers   []*arenaSolver
	Total     int32
	Purchases []string
	Balance   int
}

type arenaSolver struct {
	Name     string
	Problems []bool
}

func (h *handler) handleArena(w http.ResponseWriter, r *http.Request, _ httprouter.Params) {
	handle(w, r, func(ctx context.Context) error {
		purchase := r.URL.Query().Get("purchase")

		ss, err := h.queryAllSolutions(ctx, purchase)
		if err != nil {
			return err
		}

		problems := make([]string, 300)
		problemMap := make(map[string]int)
		for i := range problems {
			name := fmt.Sprintf("prob-%03d", i+1)
			problems[i] = name
			problemMap[name] = i
		}

		solverMap := make(map[string]*arenaSolver)
		for _, s := range ss {
			solverMap[s.Solver] = &arenaSolver{
				Name:     s.Solver,
				Problems: make([]bool, len(problems)),
			}
		}
		var solvers []*arenaSolver
		for _, s := range solverMap {
			solvers = append(solvers, s)
		}
		sort.Slice(solvers, func(i, j int) bool {
			return solvers[i].Name < solvers[j].Name
		})

		bestScores := make(map[string]int32)
		for _, s := range ss {
			if b, ok := bestScores[s.Problem]; !ok {
				bestScores[s.Problem] = s.Score
			} else if s.Score < b {
				bestScores[s.Problem] = s.Score
			}
		}

		var total int32
		for _, score := range bestScores {
			total += score
		}

		for _, s := range ss {
			if s.Score == bestScores[s.Problem] {
				solverMap[s.Solver].Problems[problemMap[s.Problem]] = true
			}
		}

		var validSolvers []*arenaSolver
		for _, s := range solvers {
			ok := false
			for _, p := range s.Problems {
				if p {
					ok = true
					break
				}
			}
			if ok {
				validSolvers = append(validSolvers, s)
			}
		}

		ps, err := h.queryPurchases(ctx)
		if err != nil {
			return err
		}

		v := &arenaValues{
			Purchase:  purchase,
			Problems:  problems,
			Solvers:   validSolvers,
			Total:     total,
			Purchases: ps,
			Balance:   loadBalance(),
		}
		return arenaTmpl.Execute(w, v)
	})
}

var problemTmpl = template.Must(parseTemplate("base.html", "problem.html"))

type problemValues struct {
	Problem      string
	Purchase     string
	Solutions    []*solution
	BestSolution *solution
	Purchases    []string
	Balance      int
}

func (h *handler) handleProblem(w http.ResponseWriter, r *http.Request, ps httprouter.Params) {
	problem := ps.ByName("problem")

	handle(w, r, func(ctx context.Context) error {
		purchase := r.URL.Query().Get("purchase")

		ss, err := h.querySolutionsByProblem(ctx, problem, purchase)
		if err != nil {
			return err
		}

		ps, err := h.queryPurchases(ctx)
		if err != nil {
			return err
		}

		var bs *solution
		if len(ss) > 0 {
			bs = ss[0]
		}
		v := &problemValues{
			Problem:      problem,
			Purchase:     purchase,
			Solutions:    ss,
			BestSolution: bs,
			Purchases:    ps,
			Balance:      loadBalance(),
		}
		return problemTmpl.Execute(w, v)
	})
}

func (h *handler) handleZip(w http.ResponseWriter, r *http.Request, _ httprouter.Params) {
	handle(w, r, func(ctx context.Context) error {
		ss, err := h.queryOptimalSolutions(ctx)
		if err != nil {
			return err
		}

		type entry struct {
			problem, purchase, solution string
		}

		ch := make(chan *entry, len(ss))

		g, ctx := errgroup.WithContext(context.Background())
		lim := concurrent.NewLimit(10)
		for _, s := range ss {
			s := s
			g.Go(func() error {
				defer lim.Use(1)()
				obj := h.cl.Bucket("sound-type-system").Object(fmt.Sprintf("solutions/%d/solution.txt", s.ID))
				r, err := obj.NewReader(ctx)
				if err != nil {
					return fmt.Errorf("failed to open %s: %v", obj.ObjectName(), err)
				}
				defer r.Close()
				b, err := ioutil.ReadAll(r)
				if err != nil {
					return fmt.Errorf("failed to read %s: %v", obj.ObjectName(), err)
				}
				ch <- &entry{
					problem:  s.Problem,
					purchase: s.Purchase,
					solution: string(b),
				}
				return nil
			})
		}

		if err := g.Wait(); err != nil {
			return err
		}

		close(ch)

		now := time.Now().In(jst)
		w.Header().Set("Content-Type", "application/zip")
		w.Header().Set("Content-Disposition", fmt.Sprintf("attachment; filename=\"submission-%s.zip\"", now.Format("20060102-150405")))
		z := zip.NewWriter(w)
		defer z.Close()

		for e := range ch {
			g, err := z.Create(fmt.Sprintf("%s.sol", e.problem))
			if err != nil {
				return err
			}
			g.Write([]byte(e.solution))
			if e.purchase != "" {
				b, err := z.Create(fmt.Sprintf("%s.buy", e.problem))
				if err != nil {
					return err
				}
				b.Write([]byte(e.purchase))
			}
		}
		return nil
	})
}

func (h *handler) handleCSV(w http.ResponseWriter, r *http.Request, _ httprouter.Params) {
	handle(w, r, func(ctx context.Context) error {
		purchases, err := h.queryPurchases(ctx)
		if err != nil {
			return err
		}

		var bests []*solution
		for _, p := range purchases {
			ss, err := h.queryBestSolutions(ctx, p)
			if err != nil {
				return err
			}
			bests = append(bests, ss...)
		}

		now := time.Now().In(jst)
		w.Header().Set("Content-Type", "text/csv")
		w.Header().Set("Content-Disposition", fmt.Sprintf("attachment; filename=\"solutions-%s.csv\"", now.Format("20060102-150405")))
		w.WriteHeader(http.StatusOK)
		cw := csv.NewWriter(w)
		for _, s := range bests {
			cw.Write([]string{s.Problem, s.Purchase, strconv.Itoa(s.ID), s.Solver, strconv.Itoa(int(s.Score))})
		}
		cw.Flush()
		return nil
	})
}

func (h *handler) queryOptimalSolutions(ctx context.Context) ([]*solution, error) {
	lock, err := os.Create("/tmp/metasolver.lock")
	if err != nil {
		return nil, err
	}
	defer lock.Close()
	if err := syscall.Flock(int(lock.Fd()), syscall.LOCK_EX); err != nil {
		return nil, err
	}
	defer syscall.Flock(int(lock.Fd()), syscall.LOCK_UN)

	balance := loadBalance()

	cmd := exec.CommandContext(ctx, "./run.sh", strconv.Itoa(balance))
	cmd.Dir = "../metasolver"
	out, err := cmd.Output()
	if err != nil {
		return nil, err
	}

	records, err := csv.NewReader(bytes.NewBuffer(out)).ReadAll()
	if err != nil {
		return nil, err
	}

	var ids []int
	for _, r := range records {
		if len(r) < 3 {
			return nil, errors.New("malformed CSV output")
		}
		id, err := strconv.Atoi(r[2])
		if err != nil {
			return nil, err
		}
		ids = append(ids, id)
	}

	idifs := make([]interface{}, len(ids))
	for i, id := range ids {
		idifs[i] = id
	}

	rows, err := h.db.QueryContext(ctx, fmt.Sprintf(`
SELECT
  id,
  solver,
  problem,
  purchase,
  evaluator,
  score,
  submitted
FROM solutions
WHERE
  id IN (%s)
ORDER BY problem ASC
`, strings.TrimRight(strings.Repeat("?, ", len(ids)), ", ")), idifs...)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	return scanSolutions(rows)
}

func (h *handler) queryBestSolutions(ctx context.Context, purchase string) ([]*solution, error) {
	rows, err := h.db.QueryContext(ctx, `
SELECT
  id,
  solver,
  problem,
  purchase,
  evaluator,
  score,
  submitted
FROM solutions
INNER JOIN (
  SELECT
    MIN(id) AS id
  FROM solutions
  INNER JOIN (
    SELECT
      problem,
      MIN(score) AS min_score
    FROM solutions
    WHERE valid AND purchase = ?
    GROUP BY problem
  ) t1 USING (problem)
  WHERE valid AND purchase = ? AND score = min_score
  GROUP BY problem
) t2 USING (id)
ORDER BY problem ASC
`, purchase, purchase)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	return scanSolutions(rows)
}

func (h *handler) querySolutionsByProblem(ctx context.Context, problem, purchase string) ([]*solution, error) {
	rows, err := h.db.QueryContext(ctx, `
SELECT
  id,
  solver,
  problem,
  purchase,
  evaluator,
  score,
  submitted
FROM solutions
WHERE
  valid AND
  problem = ? AND
  purchase = ?
ORDER BY score ASC, id ASC
`, problem, purchase)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	return scanSolutions(rows)
}

func scanSolutions(rows *sql.Rows) ([]*solution, error) {
	var ss []*solution
	for rows.Next() {
		var s solution
		if err := rows.Scan(&s.ID, &s.Solver, &s.Problem, &s.Purchase, &s.Evaluator, &s.Score, &s.Submitted); err != nil {
			return nil, err
		}
		s.Size = problemSizes[s.Problem]
		ss = append(ss, &s)
	}
	return ss, nil
}

func (h *handler) queryPurchases(ctx context.Context) ([]string, error) {
	rows, err := h.db.QueryContext(ctx, `
SELECT DISTINCT
  purchase
FROM solutions
WHERE valid
ORDER BY purchase ASC
`)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var purchases []string
	for rows.Next() {
		var p string
		if err := rows.Scan(&p); err != nil {
			return nil, err
		}
		purchases = append(purchases, p)
	}
	return purchases, nil
}

func (h *handler) queryAllSolutions(ctx context.Context, purchase string) ([]*solution, error) {
	rows, err := h.db.QueryContext(ctx, `
SELECT
  id,
  solver,
  problem,
  purchase,
  evaluator,
  score,
  submitted
FROM solutions
WHERE valid AND purchase = ?
`, purchase)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	return scanSolutions(rows)
}

func loadBalance() int {
	const (
		defaultBalance = 345331 // as of block 64
		blocksDir      = "../autominer/blocks"
	)

	fis, err := ioutil.ReadDir(blocksDir)
	if err != nil {
		return defaultBalance
	}

	balance := defaultBalance
	for _, fi := range fis {
		b, err := ioutil.ReadFile(filepath.Join(blocksDir, fi.Name(), "balances.json"))
		if err != nil {
			continue
		}
		var m map[string]int
		if err := json.Unmarshal(b, &m); err != nil {
			continue
		}
		if bl, ok := m["78"]; ok && bl > balance {
			balance = bl
		}
	}
	return balance
}
