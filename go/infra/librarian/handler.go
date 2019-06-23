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
	"io/ioutil"
	"net/http"
	"net/url"
	"os"
	"regexp"
	"strconv"
	"strings"

	"cloud.google.com/go/storage"
)

const apiKey = "c868a5215b6bfb6161c6a43363e62d45"

const checkerURL = "https://us-central1-sound-type-system.cloudfunctions.net/check"

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
	Purchase string `json:"purchase"`
	Solution string `json:"solution"`
	Score    int    `json:"score"`

	solutionHash string
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
		if err := validatePurchase(s.Purchase); err != nil {
			return err
		}
		if s.Solution == "" {
			return errors.New("solution must not be empty")
		}

		hash := sha256.Sum256([]byte(s.Solution))
		s.solutionHash = hex.EncodeToString(hash[:])

		if err := h.checkDup(ctx, &s); err != nil {
			return err
		}

		if err := h.validate(ctx, &s); err != nil {
			return err
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

func (h *handler) checkDup(ctx context.Context, s *submitRequest) error {
	row := h.db.QueryRowContext(ctx, `
SELECT 1
FROM solutions
WHERE
  solver = ? AND
  problem = ? AND
  purchase = ? AND
  solution_hash = ?
`, s.Solver, s.Problem, s.Purchase, s.solutionHash)
	var i int
	if err := row.Scan(&i); err == nil {
		return errors.New("duplicate solution")
	} else if err != sql.ErrNoRows {
		return err
	}
	return nil
}

var successRe = regexp.MustCompile(`Success!\s*Your solution took (\d+) time units`)

func (h *handler) validate(ctx context.Context, s *submitRequest) error {
	// TODO: Cache problems.
	obj := h.cl.Bucket("sound-type-system").Object(fmt.Sprintf("problems/%s.desc", s.Problem))
	r, err := obj.NewReader(ctx)
	if err != nil {
		return err
	}
	defer r.Close()

	b, err := ioutil.ReadAll(r)
	if err != nil {
		return err
	}
	problem := string(b)

	form := make(url.Values)
	form.Set("task", problem)
	form.Set("solution", s.Solution)
	form.Set("purchase", s.Purchase)
	res, err := http.PostForm(checkerURL, form)
	if err != nil {
		return err
	}
	defer res.Body.Close()

	b, err = ioutil.ReadAll(res.Body)
	if err != nil {
		return err
	}
	out := string(b)

	m := successRe.FindStringSubmatch(out)
	if m == nil {
		return fmt.Errorf("solution validation failed: %s", strings.TrimSpace(out))
	}

	score, err := strconv.Atoi(m[1])
	if err != nil {
		return err
	}

	s.Score = score
	return nil
}

func (h *handler) submit(ctx context.Context, s *submitRequest) (id, lastBestScore int, _ error) {
	tx, err := h.db.BeginTx(ctx, &sql.TxOptions{Isolation: sql.LevelSerializable})
	if err != nil {
		return 0, 0, err
	}
	defer tx.Rollback()

	row := tx.QueryRowContext(ctx, `
SELECT IFNULL(MIN(score), 0) FROM solutions WHERE valid AND problem = ? AND purchase = ?`, s.Problem, s.Purchase)
	if err := row.Scan(&lastBestScore); err != nil {
		return 0, 0, err
	}

	r, err := tx.ExecContext(ctx, `INSERT INTO solutions
(solver, problem, purchase, evaluator, solution_hash, score)
VALUES (?, ?, ?, ?, ?, ?)
`, s.Solver, s.Problem, s.Purchase, "none", s.solutionHash, s.Score)
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
