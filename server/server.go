package main

import (
	"fmt"
	"log"
	"net/http"
	"os"
	"time"
)

/*
    data format in binary:

	1 byte  = data format (0x00)
	1 byte  = reserved (0x00)
	2 bytes = obfuscation key (e.g. 0x1234)
	2 bytes = data size
	2 bytes = device id
    2 bytes = temperature (in 0.1C, signed)
	2 bytes = humidity (in 0.1%, unsigned)
    8 bytes = partial SHA256 hash of data (from byte 2 to 11)

	All data then converted from 20 bytes into 27 chars of base64url
	(see RFC 4648 section 5)
*/

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
