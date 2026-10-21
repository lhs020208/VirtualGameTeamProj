#include "flow/contract.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
int main(int argc,char** argv){try{
 if(argc!=2&&argc!=3)throw std::invalid_argument("usage: flow_contract INPUT.flow [NEW_REPORT.json]");
 const std::filesystem::path input=argv[1];if(std::filesystem::file_size(input)>65536)throw std::invalid_argument("input exceeds 64 KiB");
 std::ifstream f(input,std::ios::binary);if(!f)throw std::runtime_error("cannot open input");std::string text((std::istreambuf_iterator<char>(f)),{});if(f.bad())throw std::runtime_error("cannot read input");
 const auto c=flow::parse_contract(text);const auto report=flow::contract_report_json(c);
 if(argc==2){std::cout<<report;return 0;}
 const std::filesystem::path output=argv[2];if(std::filesystem::exists(output))throw std::invalid_argument("refusing to overwrite existing output");
 auto temp=output;temp+=".tmp";if(std::filesystem::exists(temp))throw std::invalid_argument("temporary output already exists");
 {std::ofstream out(temp,std::ios::binary);if(!out)throw std::runtime_error("cannot create output");out<<report;out.close();if(!out)throw std::runtime_error("cannot finish output");}
 std::filesystem::rename(temp,output);std::cout<<output.string()<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<"CONTRACT_REJECTED: "<<e.what()<<'\n';return 2;}}
