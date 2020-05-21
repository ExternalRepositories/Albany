close all; 
clear all; 

%define parameters 
kappa1 = 1.6; %thermal conductivity in x 
kappa2 = 0.8; %thermal conductivity in y
c = 1.0; %heat capacity 
rho = 1.0; %density 
a = 16.0;
dt = 0.01; %time step
tf = 1.0; %final time 
t = [0:dt:tf]; 
dx = 1/10; dy = 1/10;  
[xorig,yorig] = meshgrid(0:dx:1, 0:dx:1);
szx = size(xorig);
szy = size(yorig); 
x = reshape(xorig, 1, szx(1)*szx(2)); 
y = reshape(yorig, 1, szy(1)*szy(2)); 
T = ncread('thermal2D_with_source_out.exo', 'vals_nod_var1'); 
%size(T)
Texact = 0*T; 
for i=1:length(x)
  Texact(i,:) = a*x(i)*(1.0-x(i))*y(i)*(1.0-y(i))*cos(2.0*pi*kappa1*t/rho/c).*exp(2.0*pi*kappa2*t/rho/c); 
end  
%size(Texact) 

K = length(t); 
fig1=figure(1);
winsize = get(fig1,'Position');
winsize(1:2) = [0 0];
Movie=moviein(K,fig1,winsize);
set(fig1,'NextPlot','replacechildren')

rel_err = []; 
 
for i=1:K
  err = norm(T(:,i)-Texact(:,i))/norm(Texact(:,i)); 
  rel_err = [rel_err; err];
  set(gca,'NextPlot','replacechildren') ; 
  subplot(1,2,1);  
  surf(xorig, yorig, reshape(T(:,i), szx(1), szx(2)));
  minT = min(T(:,i)); 
  maxT = max(T(:,i));  
  %shading interp;
  xlabel('x'); 
  ylabel('y'); 
  axis([0 1 0 1 minT maxT])
  title(['Computed temp soln at t = ', num2str(t(i))]); 
  subplot(1,2,2); 
  surf(xorig, yorig, reshape(Texact(:,i), szx(1), szx(2))); 
  %shading interp;
  xlabel('x'); 
  ylabel('y'); 
  title(['Exact temp soln at t = ', num2str(t(i))]); 
  axis([0 1 0 1 minT maxT])
  pause(0.1)
  %Movie(:,i)=getframe(fig1,winsize);
  %mov(i) = getframe(gcf);
  %n = strcat("thermal_", num2str(i)); 
  %name = strcat(n, ".png") 
  %saveas(gcf,name)
end 

%To convert the pngs to mp4, run:
%  ffmpeg -framerate 10 -start_number 1 -i thermal_%1d.png -r 5 -vf scale=-2:1080,setsar=1 -pix_fmt yuv420p thermal_MMS.mp4

X = ['Average relative error over time = ', num2str(mean(rel_err))]; 
disp(X) 
disp([' ']); 
figure(); 
plot(t, rel_err); 
xlabel('time'); 
ylabel('Relative Error'); 
title('Relative Errors w.r.t. Exact Solution');  


