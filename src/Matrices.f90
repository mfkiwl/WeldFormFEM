module Matrices
use Domain
use ElementData

implicit none 

contains 

! subroutine printarray(Grid)
  ! implicit none
  ! CHARACTER(len=1) :: Grid(3,2)
  ! integer :: i
  ! Grid = "x"
  ! do i = 1, ubound(Grid, 1)
     ! print *, Grid(i, :)
  ! end do
! end subroutine

function det (a)
  real(fp_kind), dimension(dim,dim), intent (in) :: a 
  real(fp_kind) :: det
  !if (dim .eq. 2) then
  det = a(1,1)*a(2,2)-a(1,2)*a(2,1)
  !end if
end function

function invmat (a)
  real(fp_kind), dimension(dim,dim), intent (in) :: a 
  real(fp_kind), dimension(dim,dim) :: invmat 
  !if (dim .eq. 2) then
  invmat(1,:) = 1.0d0/(det(a))*[ a(2,2),-a(1,2)]
  invmat(2,:) = 1.0d0/(det(a))*[-a(2,1), a(1,1)]
  !end if
end function


!!!! STIFNESS MATRICES 
subroutine calculate_element_matrices ()
  integer :: e
  ! !rg=gauss[ig]
  ! !sg=gauss[jg]
  real(fp_kind), dimension(dim,nodxelem) :: dHrs
  real(fp_kind), dimension(nodxelem,dim) :: x2
  real(fp_kind), dimension(dim,dim) :: test
  real(fp_kind), dimension(dim, dim*nodxelem) :: temph
  
  integer :: i,j,k, gp
  real(fp_kind), dimension(2) :: r, s

  r(1) = -1.0/sqrt(3.0); r(2) = -r(1)
  s(1) = r(1)          ; s(2) =  r(2)
  !! Update x2 vector (this is useful for strain and stress things)
  

  do e=1, elem_count
    print *, "el ", e 
    
    elem%matkl(e,:,:) = 0.0
    elem%matknl(e,:,:) = 0.0
    if (calc_m .eqv. .True.) then
      elem%matm(e,:,:) = 0.0
    end if 
    
    print *, "nodxelem ", nodxelem
    
    i=1
    do while (i.le.nodxelem)
        print *, "elnod " , elem%elnod(e,i)
        x2(i,:)=nod%x(elem%elnod(e,i),:)
        i = i+1
    end do
    !print *, "x2 ", x2
    !printarray(x2)
    ! TODO: This could be done once
    gp = 1
    i = 1; j = 1 !TODO: CHANGE TO PLAIN DO (IN ORDER TO INCLUDE 3D)
    do while (i<=2)
      j = 1
      do while (j<=2)
        !TODO: DO THIS ONCE AT THE BEGINING ONLY ONCE FOR EACH ELEMENT TYPE
        dHrs(1,:)=[(1+s(j)),-(1+s(j)),-(1-s(j)),(1-s(j))]
        dHrs(2,:)=[(1+r(i)), (1-r(i)),-(1-r(i)),-(1+r(i))]   
        dHrs(:,:) = dHrs(:,:)*0.25

        print *, "dHrs ", dHrs
        test = matmul(dHrs,x2)
        print *, "x2, ", x2
        elem%jacob(e,gp,:,:) = test
        elem%dHxy(e,gp,:,:) = matmul(invmat(test),dHrs) !Bathe 5.25
        print *, "inv mat", elem%dHxy(e,gp,:,:)
        
        !DERIVATIVE MATRICES
        !TODO: CHANGE IF DIM != 2
        k=1
        do while (k<nodxelem)
          elem%bl(e,gp,1,dim*(k-1)+k  ) = elem%dHxy(e,gp,1,k)
          elem%bl(e,gp,2,dim*(k-1)+k+1) = elem%dHxy(e,gp,2,k)
          elem%bl(e,gp,3,dim*(k-1)+k  ) = elem%dHxy(e,gp,2,k) 
          elem%bl(e,gp,3,dim*(k-1)+k+1) = elem%dHxy(e,gp,1,k)     

          elem%bnl(e,gp,1,dim*(k-1)+k  ) = elem%dHxy(e,gp,1,k)
          elem%bnl(e,gp,2,dim*(k-1)+k  ) = elem%dHxy(e,gp,2,k)
          elem%bnl(e,gp,3,dim*(k-1)+k+1) = elem%dHxy(e,gp,1,k) 
          elem%bnl(e,gp,4,dim*(k-1)+k+1) = elem%dHxy(e,gp,2,k)     
          k = k+1
        end do
        print *, "jacob e ", elem%jacob(e,gp,:,:)
        
        elem%detJ(e) = det(elem%jacob(e,gp,:,:))
        print *, "det J", elem%detJ(e)
        !print *, "bl ", elem%bl(e,gp,:,:)
        !TODO CHANGE ZERO

        if (dim .eq. 2) then
          temph(1,:) = 0.25*[(1+r(i))*(1+s(j)),0.0d0,(1.0-r(i))*(1+s(j)),0.0d0,(1-r(i))*(1-s(j)),0.0d0,(1+r(i))*(1-s(j)),0.0d0]
          k = 1
          do while (k <= nodxelem)
            temph(2,2*k) = temph(1,2*k-1) !TODO: CHANGE IN 3D
            k = k + 1
          end do
        end if 
        elem%math(e,gp,:,:) = elem%math(e,gp,:,:) + temph(:,:)*elem%detJ(e)
        print *, "mat h ", elem%math(e,gp,:,:)
        !print *, "BL ", elem%bl
        elem%matknl(e,:,:) = elem%matknl(e,:,:) + matmul(matmul(transpose(elem%bnl(e,gp,:,:)),elem%tau(e,gp,:,:)),&
                              &elem%bnl(e,gp,:,:))*elem%detJ(e) !Nodal Weight mat
        elem%matkl(e,:,:) = elem%matkl(e,:,:) + matmul(matmul(transpose(elem%bl(e,gp,:,:)),mat_C),elem%bl(e,gp,:,:))*elem%detJ(e) !Nodal Weight mat
        if (calc_m .eqv. .True.) then
          elem%matm (e,:,:) = elem%matm (e,:,:) + matmul(transpose(elem%math(e,gp,:,:)),elem%math(e,gp,:,:)) *elem%detJ(e)!Mass matrix
        end if
        ! print *, "element mat m ", elem%matm (e,:,:)
        gp = gp + 1
        j = j +1
      end do
      i = i +1
    end do
    
    print *, "element mat m ", elem%matm (e,:,:)
    
    e = e + 1 
    ! #Numerated as in Bathe
    ! Ns  =0.25*matrix([(1+sg)*(1+rg),(1-rg)*(1+sg),(1-sg)*(1-rg),(1-sg)*(1+rg)])   
    ! dHrs=matrix([[(1+sg),-(1+sg),-(1-sg),(1-sg)], [(1+rg),(1-rg),-(1-rg),-(1+rg)] ])
    ! #Numerated as in deal.ii
    ! #dHrs=matrix([[-(1-s),(1-s),-(1+s),(1+s)], [-(1-r),-(1+r),(1-r),(1+r)] ])        
    ! dHrs/=4.
    ! J=dHrs*X2
    ! dHxy=linalg.inv(J)*dHrs
    ! detJ=linalg.det(J)
  end do
end subroutine

subroutine assemble_mass_matrix ()
  integer :: e,gp, i, j, n, n2, iglob, jglob
  
  m_glob (:,:) = 0.0d0
  do e = 1, elem_count
    print *, "elem ", e
    do n = 1, nodxelem
      do n2 = 1, nodxelem
        do i=1,dim 
          do j=1, dim

            print *, "elem ", e, "node ", n, " i j matm ",i, j, elem%matm (e,dim*(n-1)+i,dim*(n2-1)+j)
            
            iglob  = dim * (elem%elnod(e,n) - 1 ) + i
            jglob  = dim * (elem%elnod(e,n2) - 1 ) + j
            print *, "iloc, jloc ",dim*(n-1)+i, dim*(n2-1)+j, "iglob, jglob", iglob,jglob
            m_glob(iglob,jglob) = m_glob(iglob,jglob) + elem%matm (e,dim*(n-1)+i,dim*(n2-1)+j)
          end do
        end do !element row
      end do !n2
    end do ! Element node
  end do ! e
end subroutine

!NEEDED FOR STRAIN AND INTERNAL FORCES CALCULATION
!IS REALLY NEEDED TO STORE?
subroutine disassemble_uele()
  integer :: e, i, n
  do e=1,elem_count
    !print *, "elem ", e
    do n =1,nodxelem
      do i =1, dim 
        !print *, "e ", e, "n ", n, "uele estirado", 2*(n-1)+i , ",global ",elem%elnod(e,n)      
        elem%uele (e,2*(n-1)+i,1) = nod%u(elem%elnod(e,n),i)
      end do
    end do ! Element node
  end do ! e
end subroutine

subroutine assemble_int_forces()
  integer :: e, i, n, iglob
  real(fp_kind), dimension(nodxelem*dim,1) :: utemp, rtemp
  
  print *, "assemblying int forces"
  rint_glob (:,:) = 0.0d0
  e = 1
  do while (e .le. elem_count)
    !print *, "elem ", e
    rtemp = matmul(elem%matkl(e,:,:) + elem%matknl(e,:,:), elem%uele(e,:,:))
    n = 1
    do while (n .le. nodxelem)
      i = 1
      print *,"elem mat kl", elem%matkl(e,:,:)
      do while (i .le. dim )
        iglob  = dim * (elem%elnod(e,n) - 1 ) + i
        rint_glob(elem%elnod(e,n),i) =  rint_glob(elem%elnod(e,n),i) + rtemp(dim*(n-1)+i,1)
        i = i + 1
      end do !element row
      n = n + 1
    end do ! Element node
    e = e + 1
  end do ! e
end subroutine 


end module Matrices