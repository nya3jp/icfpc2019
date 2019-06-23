package tasklet

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"net/http"
)

type Task struct {
	Cmd  string `json:"cmd"`
	Pkgs []*Pkg `json:"pkgs"`
	Out  string `json:"out"`
}

type Pkg struct {
	URL  string `json:"url"`
	Dest string `json:"dest,omitempty"`
}

type Client struct {
	cl      *http.Client
	execKey string
}

func NewClient(cl *http.Client, execKey string) *Client {
	return &Client{cl, execKey}
}

func (c *Client) Run(ctx context.Context, t *Task) error {
	body, err := json.Marshal(t)
	if err != nil {
		return err
	}

	req, err := http.NewRequest(http.MethodPost, "https://tasklet-r336oz7mza-uc.a.run.app/exec", bytes.NewReader(body))
	if err != nil {
		return err
	}
	req.Header.Set("X-API-Key", c.execKey)
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
