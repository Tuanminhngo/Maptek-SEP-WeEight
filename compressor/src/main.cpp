#include <iostream>
#include "App.hpp"

int main(){
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);

  try{
    App app;
    // To force RLEX baseline instead, uncomment:
    // app.kind = Strategy::Kind::RLEX;

    return app.run();
  }catch(const std::exception& e){
    std::cerr<<"INPUT/PROCESS ERROR: "<<e.what()<<"\n";
    return 2;
  }
}
