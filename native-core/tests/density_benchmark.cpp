#include "kivapfa/MidiParser.h"
#include "kivapfa/PagedNoteStore.h"
#include "kivapfa/PlaybackCursor.h"
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>
static void b16(std::vector<uint8_t>&v,uint16_t x){v.push_back(x>>8);v.push_back(x);} static void b32(std::vector<uint8_t>&v,uint32_t x){v.push_back(x>>24);v.push_back(x>>16);v.push_back(x>>8);v.push_back(x);} static void var(std::vector<uint8_t>&v,uint32_t x){uint8_t b[4];int n=0;b[n++]=x&127;while((x>>=7))b[n++]=128|(x&127);while(n)v.push_back(b[--n]);}
static void make(const char*path,int n){std::vector<uint8_t>t;t.reserve((size_t)n*8+32);for(int i=0;i<n;i++){var(t,0);t.push_back(0x90|(i&15));t.push_back((uint8_t)(21+i%88));t.push_back(100);}var(t,480);t.push_back(0x80);t.push_back(21);t.push_back(0);for(int i=1;i<n;i++){var(t,0);t.push_back(0x80|(i&15));t.push_back((uint8_t)(21+i%88));t.push_back(0);}var(t,0);t.insert(t.end(),{0xff,0x2f,0});std::vector<uint8_t>o;o.insert(o.end(),{'M','T','h','d'});b32(o,6);b16(o,0);b16(o,1);b16(o,480);o.insert(o.end(),{'M','T','r','k'});b32(o,(uint32_t)t.size());o.insert(o.end(),t.begin(),t.end());std::ofstream f(path,std::ios::binary);f.write((char*)o.data(),o.size());}
int main(int argc,char**argv){int n=argc>1?std::stoi(argv[1]):200000;make("/tmp/kd.mid",n);auto idx=kivapfa::MidiParser::buildPagedIndex("/tmp/kd.mid","/tmp/kd.page");kivapfa::PagedNoteStore s("/tmp/kd.page",idx,4096,48u*1024u*1024u);kivapfa::PlaybackCursor c(s);std::vector<float>gpu((size_t)n*4);auto a=std::chrono::steady_clock::now();auto got=c.collectPacked4(0,4'000'000,gpu.data(),n);auto b=std::chrono::steady_clock::now();std::cout<<"dense_notes="<<got<<" collect_ms="<<std::chrono::duration<double,std::milli>(b-a).count()<<"\n";}
