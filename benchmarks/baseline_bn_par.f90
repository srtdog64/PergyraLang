module bnmod
  implicit none
contains
  subroutine process_perm(perm, n, hist, nxt, visited)
    integer, intent(in) :: perm(:), n
    integer(8), intent(inout) :: hist(:)
    integer, intent(inout) :: nxt(:), visited(:)
    integer :: twoN, limit, mask, m, i, bit, v, r, cycles, s, cur
    twoN = 2*n
    limit = 1
    do i = 1, n
       limit = limit * 2
    end do
    do mask = 0, limit-1
       m = mask
       do i = 1, n
          bit = m - (m/2)*2
          v = perm(i)
          if (bit == 0) then
             nxt(i) = v
             nxt(n+i) = n+v
          else
             nxt(i) = n+v
             nxt(n+i) = v
          end if
          m = m/2
       end do
       do r = 1, twoN
          visited(r) = 0
       end do
       cycles = 0
       do s = 1, twoN
          if (visited(s) == 0) then
             cycles = cycles + 1
             cur = s
             do while (visited(cur) == 0)
                visited(cur) = 1
                cur = nxt(cur)
             end do
          end if
       end do
       hist(cycles) = hist(cycles) + 1
    end do
  end subroutine process_perm

  recursive subroutine heapsub(m, base, perm, n, hist, nxt, visited)
    integer, intent(in) :: m, base, n
    integer, intent(inout) :: perm(:), nxt(:), visited(:)
    integer(8), intent(inout) :: hist(:)
    integer :: i, t
    if (m == 1) then
       call process_perm(perm, n, hist, nxt, visited)
       return
    end if
    do i = 1, m-1
       call heapsub(m-1, base, perm, n, hist, nxt, visited)
       if (mod(m,2) == 0) then
          t = perm(base+i-1); perm(base+i-1) = perm(base+m-1); perm(base+m-1) = t
       else
          t = perm(base); perm(base) = perm(base+m-1); perm(base+m-1) = t
       end if
    end do
    call heapsub(m-1, base, perm, n, hist, nxt, visited)
  end subroutine heapsub

  function compute_prefix(first, n) result(cnt)
    integer, intent(in) :: first, n
    integer(8) :: cnt
    integer, allocatable :: perm(:), nxt(:), visited(:)
    integer(8), allocatable :: hist(:)
    integer :: twoN, i, v, idx
    twoN = 2*n
    allocate(perm(n), nxt(twoN), visited(twoN), hist(twoN))
    perm(1) = first
    idx = 2
    do v = 1, n
       if (v /= first) then
          perm(idx) = v
          idx = idx + 1
       end if
    end do
    hist = 0
    call heapsub(n-1, 2, perm, n, hist, nxt, visited)
    cnt = 0
    do i = 1, twoN
       cnt = cnt + hist(i)
    end do
    deallocate(perm, nxt, visited, hist)
  end function compute_prefix
end module bnmod

program bn_par
  use bnmod
  implicit none
  integer :: n, first, nargs
  integer(8) :: total
  character(len=32) :: arg
  n = 10
  nargs = command_argument_count()
  if (nargs >= 1) then
     call get_command_argument(1, arg)
     read(arg, *) n
  end if
  total = 0
  !$omp parallel do reduction(+:total) schedule(dynamic)
  do first = 1, n
     total = total + compute_prefix(first, n)
  end do
  !$omp end parallel do
  print '(A,I0)', 'total=', total
end program bn_par
