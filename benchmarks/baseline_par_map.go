// baseline_par_map.go -- Go twins for the map benchmarks.
// Modes:
//
//	chunked <n>   16 goroutines over disjoint ranges (idiomatic throughput)
//	perelem <n>   one goroutine per element + atomic add (cheap-task
//	              representative; the fine-axis twin)
//
// Build: go build -o baseline_par_map_go baseline_par_map.go
package main

import (
	"fmt"
	"os"
	"strconv"
	"sync"
	"sync/atomic"
)

func body(i int64) int64 { return ((i % 1000) * 31 + 7) % 100 }

func main() {
	if len(os.Args) < 3 {
		fmt.Fprintln(os.Stderr, "usage: chunked|perelem <n>")
		os.Exit(2)
	}
	mode := os.Args[1]
	n, _ := strconv.ParseInt(os.Args[2], 10, 64)
	var total int64

	switch mode {
	case "chunked":
		const chunks = 16
		var wg sync.WaitGroup
		partial := make([]int64, chunks)
		size := n / chunks
		for c := 0; c < chunks; c++ {
			wg.Add(1)
			go func(c int) {
				defer wg.Done()
				lo, hi := int64(c)*size, int64(c+1)*size
				var acc int64
				for i := lo; i < hi; i++ {
					acc += body(i)
				}
				partial[c] = acc
			}(c)
		}
		wg.Wait()
		for _, p := range partial {
			total += p
		}
	case "perelem":
		var wg sync.WaitGroup
		for i := int64(0); i < n; i++ {
			wg.Add(1)
			go func(i int64) {
				defer wg.Done()
				atomic.AddInt64(&total, body(i))
			}(i)
		}
		wg.Wait()
	default:
		fmt.Fprintln(os.Stderr, "unknown mode", mode)
		os.Exit(2)
	}
	fmt.Printf("total=%d\n", total)
}
