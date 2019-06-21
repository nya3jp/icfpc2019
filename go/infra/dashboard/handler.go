package main

import (
	"context"
	"database/sql"
	"fmt"
	"html/template"
	"net/http"
	"os"

	"cloud.google.com/go/storage"
)

type handler struct {
	mux *http.ServeMux
	db  *sql.DB
	cl  *storage.Client
}

func newHandler(db *sql.DB, cl *storage.Client) http.Handler {
	mux := http.NewServeMux()
	h := &handler{
		mux: mux,
		db:  db,
		cl:  cl,
	}
	mux.HandleFunc("/dashboard", h.handleDashboard)
	return mux
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

type solution struct {
	ID        int
	Solver    string
	Problem   string
	Evaluator string
	Score     int32
}

var dashboardTmpl = template.Must(template.ParseFiles("infra/dashboard/templates/dashboard.html"))

func (h *handler) handleDashboard(w http.ResponseWriter, r *http.Request) {
	handle(w, r, func(ctx context.Context) error {
		ss, err := h.queryBestSolutions(ctx)
		if err != nil {
			return err
		}

		return dashboardTmpl.Execute(w, ss)
	})
}

func (h *handler) queryBestSolutions(ctx context.Context) ([]*solution, error) {
	rows, err := h.db.QueryContext(ctx, `
SELECT
  id,
  solver,
  problem,
  evaluator,
  score
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
    WHERE valid
    GROUP BY problem
  ) t1 USING (problem)
  WHERE score = min_score
  GROUP BY problem
) t2 USING (id)
ORDER BY problem ASC
`)
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
		if err := rows.Scan(&s.ID, &s.Solver, &s.Problem, &s.Evaluator, &s.Score); err != nil {
			return nil, err
		}
		ss = append(ss, &s)
	}
	return ss, nil
}
