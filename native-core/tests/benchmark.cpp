#include "kivapfa/MidiParser.h"
#include "kivapfa/PagedNoteStore.h"
#include "kivapfa/PlaybackCursor.h"
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

static void be16(std::vector<uint8_t>& v,uint16_t x){v.push_back(x>>8);v.push_back(x);}
static void be32(std::vector<uint8_t>& v,uint32_t x){v.push_back(x>>24);v.push_back(x>>16);v.push_back(x>>8);v.push_back(x);}
static void var(std::vector<uint8_t>& v,uint32_t x){uint8_t b[4];int n=0;b[n++]=x&0x7f;while((x>>=7)) b[n++]=0x80|(x&0x7f);while(n) v.push_back(b[--n]);}
static void makeMidi(const char* path,int notes){
  std::vector<uint8_t> tr; tr.reserve((size_t)notes*8+16);
  for(int i=0;i<notes;i++){
    uint8_t k=(uint8_t)(21+(i%88));
    var(tr,0); tr.push_back(0x90); tr.push_back(k); tr.push_back(100);
    var(tr,1); tr.push_back(0x80); tr.push_back(k); tr.push_back(0);
  }
  var(tr,0); tr.push_back(0xff); tr.push_back(0x2f); tr.push_back(0);
  std::vector<uint8_t> out; out.insert(out.end(),{'M','T','h','d'});be32(out,6);be16(out,0);be16(out,1);be16(out,480);
  out.insert(out.end(),{'M','T','r','k'});be32(out,(uint32_t)tr.size());out.insert(out.end(),tr.begin(),tr.end());
  std::ofstream f(path,std::ios::binary);f.write((char*)out.data(),(std::streamsize)out.size());
}
int main(int argc,char**argv){
  int N=argc>1?std::stoi(argv[1]):200000;
  const char* m="/tmp/kivapfa_bench.mid";const char* p="/tmp/kivapfa_bench.page";makeMidi(m,N);
  auto t0=std::chrono::steady_clock::now();auto idx=kivapfa::MidiParser::buildPagedIndex(m,p);auto t1=std::chrono::steady_clock::now();
  kivapfa::PagedNoteStore store(p,idx,4096,48u*1024u*1024u);kivapfa::PlaybackCursor c(store);
  size_t sum=0; std::vector<float> gpu(500000*4); auto t2=std::chrono::steady_clock::now();
  for(int frame=0;frame<600;frame++) sum+=c.collectPacked4((int64_t)frame*16667,4'000'000,gpu.data(),500000);
  auto t3=std::chrono::steady_clock::now();
  auto ms=[](auto a,auto b){return std::chrono::duration<double,std::milli>(b-a).count();};
  std::cout<<"notes="<<idx.total_notes<<" parse_ms="<<ms(t0,t1)<<" frames600_ms="<<ms(t2,t3)<<" avg_frame_ms="<<ms(t2,t3)/600.0<<" visible_sum="<<sum<<"\n";
}
