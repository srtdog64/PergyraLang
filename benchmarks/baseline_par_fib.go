// baseline_par_fib.go -- Go twin for the nested fork-join benchmark.
// fib(38), serial cutoff below 28, one goroutine per left branch.
// Build: go build -o baseline_par_fib_go baseline_par_fib.go
package main

import (
	"fmt"
	"sync"
)

func fibSer(n int) int {
	if n < 2 {
		return n
	}
	return fibSer(n-1) + fibSer(n-2)
}

func fibPar(n int) int {
	if n < 28 {
		return fibSer(n)
	}
	var a int
	var wg sync.WaitGroup
	wg.Add(1)
	go func() {
		defer wg.Done()
		a = fibPar(n - 1)
	}()
	b := fibPar(n - 2)
	wg.Wait()
	return a + b
}

func main() {
	fmt.Printf("total=%d\n", fibPar(38))
}
