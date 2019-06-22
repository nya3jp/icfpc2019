package concurrent

import (
	"sync"
)

type Limit struct {
	cur, cap int
	mu       sync.Mutex
	cond     *sync.Cond
}

func NewLimit(cap int) *Limit {
	q := &Limit{
		cur: cap,
		cap: cap,
	}
	q.cond = sync.NewCond(&q.mu)
	return q
}

func (q *Limit) Use(size int) func() {
	q.mu.Lock()
	defer q.mu.Unlock()

	for q.cur < size {
		q.cond.Wait()
	}
	q.cur -= size

	return func() {
		q.mu.Lock()
		defer q.mu.Unlock()
		q.cur += size
		q.cond.Broadcast()
	}
}

func (q *Limit) Wait() {
	q.mu.Lock()
	defer q.mu.Unlock()

	for q.cur < q.cap {
		q.cond.Wait()
	}
}
