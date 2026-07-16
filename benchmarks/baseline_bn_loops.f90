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

  recursive subroutine heap(k, perm, n, hist, nxt, visited)
    integer, intent(in) :: k, n
    integer, intent(inout) :: perm(:), nxt(:), visited(:)
    integer(8), intent(inout) :: hist(:)
    integer :: i, t
    if (k == 1) then
       call process_perm(perm, n, hist, nxt, visited)
       return
    end if
    do i = 1, k-1
       call heap(k-1, perm, n, hist, nxt, visited)
       if (mod(k,2) == 0) then
          t = perm(i); perm(i) = perm(k); perm(k) = t
       else
          t = perm(1); perm(1) = perm(k); perm(k) = t
       end if
    end do
    call heap(k-1, perm, n, hist, nxt, visited)
  end subroutine heap
end module bnmod

program bn
  use bnmod
  implicit none
  integer :: n, i, twoN, nargs
  integer, allocatable :: perm(:), nxt(:), visited(:)
  integer(8), allocatable :: hist(:)
  integer(8) :: total
  character(len=32) :: arg
  n = 10
  nargs = command_argument_count()
  if (nargs >= 1) then
     call get_command_argument(1, arg)
     read(arg, *) n
  end if
  twoN = 2*n
  allocate(perm(n), nxt(twoN), visited(twoN), hist(twoN))
  do i = 1, n
     perm(i) = i
  end do
  hist = 0
  call heap(n, perm, n, hist, nxt, visited)
  total = 0
  do i = 1, twoN
     total = total + hist(i)
  end do
  print '(A,I0)', 'total=', total
end program bn
