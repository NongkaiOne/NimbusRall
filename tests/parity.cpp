#include <iostream>
#include <iomanip>
#include "features.h"
int main(){
  float mean[FEATURE_COUNT],scale[FEATURE_COUNT],coef[FEATURE_COUNT],intercept;
  for(auto& v:mean)std::cin>>v;for(auto& v:scale)std::cin>>v;for(auto& v:coef)std::cin>>v;std::cin>>intercept;
  float t,h,p,l,hour,dp1,dp3,dh1,dt1,ratio,sd;int doy;
  while(std::cin>>t>>h>>p>>l>>hour>>doy>>dp1>>dp3>>dh1>>dt1>>ratio>>sd){
    float x[FEATURE_COUNT];featureVector(t,h,p,l,hour,doy,dp1,dp3,dh1,dt1,ratio,sd,x);
    std::cout<<std::setprecision(10);for(float v:x)std::cout<<v<<' ';
    std::cout<<logistic(x,mean,scale,coef,intercept)<<' '<<fallbackProbability(x)<<'\n';
  }
}
