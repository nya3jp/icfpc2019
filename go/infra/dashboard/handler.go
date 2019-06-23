package main

import (
	"archive/zip"
	"context"
	"database/sql"
	"encoding/csv"
	"fmt"
	"html/template"
	"io/ioutil"
	"math"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
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
	r.Handle("GET", "/dashboard/problem/:problem", h.handleProblem)
	r.Handle("GET", "/dashboard/zip", h.handleZip)
	r.Handle("GET", "/dashboard/csv", h.handleCSV)
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
	return int(math.Ceil(1000 * math.Log2(float64(s.Size.Y)*float64(s.Size.Y))))
}

var indexTmpl = template.Must(parseTemplate("base.html", "index.html"))

type indexValues struct {
	Purchase      string
	BestSolutions []*solution
	Purchases     []string
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
		}
		return indexTmpl.Execute(w, v)
	})
}

var problemTmpl = template.Must(parseTemplate("base.html", "problem.html"))

type problemValues struct {
	Problem      string
	Purchase     string
	Solutions    []*solution
	BestSolution *solution
	Purchases    []string
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
		}
		return problemTmpl.Execute(w, v)
	})
}

func (h *handler) handleZip(w http.ResponseWriter, r *http.Request, _ httprouter.Params) {
	handle(w, r, func(ctx context.Context) error {
		const purchase = ""

		ss, err := h.queryBestSolutions(ctx, purchase)
		if err != nil {
			return err
		}

		type entry struct {
			problem, solution string
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
