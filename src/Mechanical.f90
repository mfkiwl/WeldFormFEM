module Mechanical
use Domain
use ElementData
use NodeData

contains
!THIS SHOULD BE DONE AT t+1/2dt
subroutine cal_elem_strains ()
  implicit none
  integer :: e, i,j,k, gp
  real(fp_kind), dimension(2) :: r, s
  
 
  r(1) = -1.0/sqrt(3.0); r(2) = -r(1)
  s(1) = r(1)          ; s(2) =  r(2)
  
  gp = 1
  do e=1, elem_count
    !Is only linear matrix?    
    elem%strain(e,gp,:,:) = matmul(elem%bl(e,gp,:,:),elem%uele (e,:,:)) 
    elem%str_rate(e,gp,:,:) = matmul(elem%bl(e,gp,:,:),elem%vele (e,:,:)) 
  end do
end subroutine


subroutine CalcStressStrain (dt) 

  implicit none
  real(fp_kind) :: RotationRateT(3,3), Stress(3,3), SRT(3,3), RS(3,3), ident(3,3)
  integer :: i
  real(fp_kind) ,intent(in):: dt
  
  real(fp_kind) :: p00
  
  p00 = 0.
  
  ident = 0.0d0
  ident (1,1) = 1.0d0; ident (2,2) = 1.0d0; ident (3,3) = 1.0d0
  
  ! !!!$omp parallel do num_threads(Nproc) private (RotationRateT, Stress, SRT, RS)
  ! do i = 1, elem_count
    ! pt%pressure(i) = EOS(0, pt%cs(i), p00,pt%rho(i), pt%rho_0(i))
    ! if (i==52) then
    ! !print *, "pt%pressure(i)", pt%pressure(i),", cs ", pt%cs(i), "p00", p00, ", rho", p00,pt%rho(i), ", rho 0", p00,pt%rho_0(i)
    ! end if
    ! RotationRateT = transpose (elem%rot_rate(e,:,:))

    ! SRT = MatMul(pt%shear_stress(i,:,:),RotationRateT)
    ! RS  = MatMul(pt%rot_rate(i,:,:), pt%shear_stress(i,:,:))
    
    ! !print *, "RS", RS
    ! pt%shear_stress(i,:,:)	= dt * (2.0 * mat_G *(pt%str_rate(i,:,:)-1.0/3.0 * &
                                 ! (pt%str_rate(i,1,1)+pt%str_rate(i,2,2)+pt%str_rate(i,3,3))*ident) &
                                 ! +SRT+RS) + pt%shear_stress(i,:,:)
    ! pt%sigma(i,:,:)			= -pt%pressure(i) * ident + pt%shear_stress(i,:,:)	!Fraser, eq 3.32
    ! !print *, "particle ", i, ", rot_rate ", pt%rot_rate(i,:,:)
    ! !pt%strain(i)			= dt*pt%str_rate(i + Strain;
  ! end do
  ! !!!!$omp end parallel do    


end subroutine CalcStressStrain

subroutine calc_elem_vol ()
  implicit none
  integer :: e, gp
  real(fp_kind):: w
  
  ! P00+(Cs0*Cs0)*(Density-Density0);
  do e = 1, elem_count
    elem%vol(e) = 0.0d0
    if (elem%gausspc(e).eq.1) then
      w = 8.0
    end if
    do gp=1,elem%gausspc(e)
      !elem%vol(e) = 
      print *, "elem e j", elem%detJ(e,gp)
      elem%vol(e) = elem%vol(e) + elem%detJ(e,gp)*w
      end do !gp  
    print *, "Elem ", e, "vol ",elem%vol(e)
  end do

end subroutine

!!!!!EOS: Equation of State
subroutine calc_elem_density ()
  implicit none
  integer :: e
  ! P00+(Cs0*Cs0)*(Density-Density0);
  do e = 1, elem_count
    !elem%rho(e) = 
  end do

end subroutine


subroutine impose_bcv
  implicit none
  integer :: n, d
  n = 1
  do while (n .le. node_count)    
    d = 1
    do while (d .le. 2)
      if (nod%is_bcv(n,d) .eqv. .true.) then
        nod%v(n,d) = nod%bcv(n,d)
        print *, "nod ", n, ", ",nod%bcv(n,d), ", d", d
      end if
      
      if (nod%is_fix(n,d) .eqv. .true.) then
        nod%v(n,d) = 0.0
      end if 
      d = d + 1 
    end do !dim
    n = n + 1
  end do !Node    
end subroutine

subroutine impose_bca
  implicit none
  integer :: n, d
  n = 1
  do while (n .le. node_count)    
    d = 1
    do while (d .le. 2)
      ! if (nod%is_bcv(n,d) .eqv. .true.) then
        ! nod%v(n,d) = nod%bcv(n,d)
        ! print *, "nod ", n, ", ",nod%bcv(n,d), ", d", d
      ! end if
      
      if (nod%is_fix(n,d) .eqv. .true.) then
        nod%a(n,d) = 0.0
      end if 
      d = d + 1 
    end do !dim
    n = n + 1
  end do !Node    
end subroutine

end module