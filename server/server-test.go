package main

import (
	"fmt"
	"log"
	"net/http"
	"os"
	"time"
)

func handler(w http.ResponseWriter, r *http.Request) {
	now := time.Now()
	pw := r.FormValue("pw")
	if pw == "555541" {
		id := r.FormValue("id")
		tm := r.FormValue("t")
		filename := "data-" + id + ".csv"
		f, err := os.OpenFile(filename, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0600)
		if err == nil {
			fmt.Fprintf(f, "%s, %s\n", now, tm)
			f.Close()

			fmt.Fprintf(w, "%s\n", now.String())
			fmt.Fprintf(w, "id: %s\n", id)
			fmt.Fprintf(w, "temp: %s\n", tm)
		} else {
			fmt.Fprintf(w, "message igmored\n")
		}
	}
}

func main() {
	http.HandleFunc("/", handler)
	log.Fatal(http.ListenAndServe(":8080", nil))
}
