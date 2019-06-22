package main

import (
	"context"
	"crypto/sha256"
	"database/sql"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"os"

	"cloud.google.com/go/storage"
)

const apiKey = "c868a5215b6bfb6161c6a43363e62d45"

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
	mux.HandleFunc("/submit", h.handleSubmit)
	return mux
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
		w.Header().Set("Content-Type", "text/plain")
		http.Error(w, msg, http.StatusInternalServerError)
	}
}

type submitRequest struct {
	Solver   string `json:"solver"`
	Problem  string `json:"problem"`
	Solution string `json:"solution"`
	Score    int    `json:"score"`
}

type submitResponse struct {
	ID            int `json:"id"`
	Score         int `json:"score"`
	LastBestScore int `json:"lastBestScore"`
}

func (h *handler) handleSubmit(w http.ResponseWriter, r *http.Request) {
	handle(w, r, func(ctx context.Context) error {
		if r.Method != "POST" {
			return errors.New("POST only")
		}

		var s submitRequest
		if err := json.NewDecoder(r.Body).Decode(&s); err != nil {
			return err
		}
		if s.Solver == "" {
			return errors.New("solver must not be empty")
		}
		if s.Problem == "" {
			return errors.New("problem must not be empty")
		}
		if s.Solution == "" {
			return errors.New("solution must not be empty")
		}
		if s.Score == 0 {
			return errors.New("score must not be empty")
		}

		id, lastBestScore, err := h.submit(ctx, &s)
		if err != nil {
			return err
		}

		res := &submitResponse{
			ID:            id,
			Score:         s.Score,
			LastBestScore: lastBestScore,
		}
		return json.NewEncoder(w).Encode(res)
	})
}

func (h *handler) submit(ctx context.Context, s *submitRequest) (id, lastBestScore int, _ error) {
	tx, err := h.db.BeginTx(ctx, &sql.TxOptions{Isolation: sql.LevelSerializable})
	if err != nil {
		return 0, 0, err
	}
	defer tx.Rollback()

	hbin := sha256.Sum256([]byte(s.Solution))
	hash := hex.EncodeToString(hbin[:])

	row := tx.QueryRowContext(ctx, `
SELECT IFNULL(MIN(score), 0) FROM solutions WHERE valid AND problem = ?`, s.Problem)
	if err := row.Scan(&lastBestScore); err != nil {
		return 0, 0, err
	}

	r, err := tx.ExecContext(ctx, `INSERT INTO solutions
(solver, problem, evaluator, solution_hash, score)
VALUES (?, ?, ?, ?, ?)
`, s.Solver, s.Problem, "none", hash, s.Score)
	if err != nil {
		return 0, 0, err
	}

	id64, err := r.LastInsertId()
	if err != nil {
		return 0, 0, err
	}
	id = int(id64)

	wctx, cancel := context.WithCancel(ctx)
	defer cancel()

	obj := h.cl.Bucket("sound-type-system").Object(fmt.Sprintf("solutions/%d/solution.txt", id))
	w := obj.NewWriter(wctx)
	w.ContentType = "text/plain"
	if _, err := io.WriteString(w, s.Solution); err != nil {
		cancel()
		w.Close()
		return 0, 0, err
	}
	if err := w.Close(); err != nil {
		return 0, 0, err
	}

	if err := tx.Commit(); err != nil {
		return 0, 0, err
	}

	return id, lastBestScore, nil
}
