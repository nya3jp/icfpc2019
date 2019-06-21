package main

import (
	"context"
	"database/sql"
	"flag"
	"fmt"
	"net"
	"net/http"
	"os"

	"cloud.google.com/go/storage"
	_ "github.com/go-sql-driver/mysql"
)

type args struct {
	port   int
	dbPath string
}

func parseArgs() (*args, error) {
	var args args
	fs := flag.NewFlagSet("", flag.ContinueOnError)
	fs.IntVar(&args.port, "port", 8080, "port to listen")
	fs.StringVar(&args.dbPath, "db", "ts:ts@/ts", "DB path")
	return &args, fs.Parse(os.Args[1:])
}

func main() {
	if err := func() error {
		args, err := parseArgs()
		if err != nil {
			return err
		}

		db, err := sql.Open("mysql", args.dbPath+"?parseTime=true")
		if err != nil {
			return err
		}
		defer db.Close()

		cl, err := storage.NewClient(context.Background())
		if err != nil {
			return err
		}

		lis, err := net.Listen("tcp", fmt.Sprintf("0.0.0.0:%d", args.port))
		if err != nil {
			return fmt.Errorf("failed to listen: %v", err)
		}

		fmt.Fprintln(os.Stderr, "Ready")
		return http.Serve(lis, newHandler(db, cl))
	}(); err != nil {
		panic(fmt.Sprint("ERROR: ", err))
	}
}
