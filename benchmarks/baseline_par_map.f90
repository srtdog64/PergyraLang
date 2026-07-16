! baseline_par_map.f90 -- Fortran twin of the chunked/throughput map
! benchmark: OpenMP do-reduction over n elements, same body.
! Build: gfortran -O2 -fopenmp baseline_par_map.f90 -o baseline_par_map_f
program par_map
  implicit none
  integer(8) :: n, i, total
  character(len=32) :: arg
  n = 32000000_8
  if (command_argument_count() >= 1) then
     call get_command_argument(1, arg)
     read(arg, *) n
  end if
  total = 0_8
  !$omp parallel do reduction(+:total)
  do i = 0, n - 1
     total = total + mod(mod(i, 1000_8) * 31_8 + 7_8, 100_8)
  end do
  !$omp end parallel do
  print '(A,I0)', 'total=', total
end program par_map
