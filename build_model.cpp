//
//  score_string.cpp
//  PST
//
//  Created by Niklas Alanko on 24/04/2017.
//  Copyright © 2017 University of Helsinki. All rights reserved.
//

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <ctime>
#include <chrono>
#include <streambuf>
#include "String_Depth_Support.hh"
#include "Parent_Support.hh"
#include "LMA_Support.hh"
#include "BPR_tools.hh"
#include "Precalc.hh"
#include "input_reading.hh"
#include "score_string.hh"
#include "build_model.hh"
#include "logging.hh"

#define HUGE_NUMBER 1e18

using namespace std;

vector<string> split(string s, char delimiter){
    stringstream test(s);
    string segment;
    vector<string> seglist;

    while(getline(test, segment, delimiter))
    {
        seglist.push_back(segment);
    }
    return seglist;
}

string read_raw_file(string filename){

    std::ifstream t(filename.c_str());
    std::string str;

    t.seekg(0, std::ios::end);   
    str.reserve(t.tellg());
    t.seekg(0, std::ios::beg);

    str.assign((std::istreambuf_iterator<char>(t)),
                std::istreambuf_iterator<char>());
    
    return str;
}

class Build_Time_Config{
    
private:
    
  Build_Time_Config(const Build_Time_Config&); // Prevent copy-construction
  Build_Time_Config& operator=(const Build_Time_Config&);  // Prevent assignment
    
public:
    
    enum Context_Type {UNDEFINED, EQ234, ENTROPY, KL, PNORM};
    
    bool context_stats;
    bool only_maxreps;
    int64_t depth_bound;
    Context_Type context_type;
    string outputdir;
    string input_filename;
    bool run_length_encoding;
    bool store_depths;
    
    Context_Callback* cf;
    
    Iterator* rev_st_it;
    Iterator* slt_it;
    
    Build_Time_Config() : context_stats(false), only_maxreps(false), depth_bound(HUGE_NUMBER), context_type(UNDEFINED), run_length_encoding(false), store_depths(false),
                          cf(nullptr), rev_st_it(nullptr), slt_it(nullptr) {}
    
    ~Build_Time_Config(){
        delete cf;
        delete rev_st_it;
    }
    
    void assert_all_ok(){
        if(!only_maxreps && depth_bound < HUGE_NUMBER){
            cerr << "Not implemented error: Depth bounded but without maxrep pruning" << endl;
            exit(-1);
        }
        
        assert(input_filename != "");
        assert(outputdir != "");
        assert(context_type != UNDEFINED);
        assert(cf != nullptr);
        assert(rev_st_it != nullptr);
        assert(slt_it != nullptr);
    }
    
    string context_type_to_string(Context_Type ct){
        if(ct == UNDEFINED) return "undefined";
        if(ct == EQ234) return "EQ234";
        if(ct == ENTROPY) return "entropy";
        if(ct == KL) return "KL";
        if(ct == PNORM) return "pnorm";
        
        assert(false);
    }
    
    void write_to_file(string directory, string filename){
        ofstream file(directory + "/" + filename);
        
        file << only_maxreps << "\n" << context_type_to_string(context_type) << "\n" << run_length_encoding << "\n" << depth_bound << "\n";
        
        if(!file.good()){
            cerr << "Error writing to file " << filename << endl;
            exit(-1);
        }
    }
    
};

int build_model_main(int argc, char** argv){
    if(argc < 4){
        cerr << "Builds a VOMM index" << endl;
        cerr << "Usage: see README.md" << endl;
        return -1;
    }
    
    Build_Time_Config C;
    
    vector<string> queries;
    string reference;
    for(int64_t i = 1; i < argc; i++){
        if(argv[i] == string("--reference-raw")){
            i++;
            reference = read_raw_file(argv[i]);
            C.input_filename = argv[i];
        } else if(argv[i] == string("--reference-fasta")){
            // Concatenate all reads in fasta
            i++;
            auto v = parse_FASTA(argv[i]);
            for(auto pair : v) reference += pair.first;
            C.input_filename = argv[i];
        } else if(argv[i] == string("--maxreps-pruning")){
            C.rev_st_it = new Rev_ST_Maxrep_Iterator();
            C.slt_it = new SLT_Iterator();
            C.only_maxreps = true;
        } else if(argv[i] == string("--rle")){
            C.run_length_encoding = true;
        } else if(argv[i] == string("--depth")){ // Assumes also maxrep pruning
            i++;
            int64_t bound = stoi(argv[i]);
            C.rev_st_it = new Rev_ST_Depth_Bounded_Maxrep_Iterator(bound);
            C.slt_it = new Depth_Bounded_SLT_Iterator(bound);
            C.depth_bound = bound;
        } else if(argv[i] == string("--entropy")){
            i++;
            double threshold = stod(argv[i]);
            C.cf = new Entropy_Formula(threshold);
            C.context_type = Build_Time_Config::ENTROPY;
        } else if(argv[i] == string("--KL")){
            i++;
            double threshold = stod(argv[i]);
            C.cf = new KL_Formula(threshold);
            C.context_type = Build_Time_Config::KL;
        } else if(argv[i] == string("--pnorm")){
            i++;
            int64_t p = stoi(argv[i]);
            i++;
            double threshold = stod(argv[i]);
            C.cf = new pnorm_Formula(p, threshold);
            C.context_type = Build_Time_Config::PNORM;
        } else if(argv[i] == string("--four-thresholds")){
            double t1,t2,t3,t4;
            i++; t1 = stod(argv[i]);
            i++; t2 = stod(argv[i]);
            i++; t3 = stod(argv[i]);
            i++; t4 = stod(argv[i]);
            C.cf = new EQ234_Formula(t1,t2,t3,t4);
            C.context_type = Build_Time_Config::EQ234;
        } else if(argv[i] == string("--outputdir")){
            i++;
            string dir = argv[i];
            C.outputdir = dir;
        } else if(argv[i] == string("--context-stats")){
            C.context_stats = true;
        } else if(argv[i] == string("--store-depths")){
            C.store_depths = true;
        } else{
            cerr << "Invalid argument: " << argv[i] << endl;
            return -1;
        }
    }
    
    bool full_topology = !C.only_maxreps;
    if(full_topology){
        C.rev_st_it = new Rev_ST_Iterator();
        C.slt_it = new SLT_Iterator();
    }
    
    C.assert_all_ok();
    
    string filename = split(C.input_filename,'/').back();
    write_log("Starting to build the model");
    Global_Data G;
    Stats_writer wr;
    if(C.context_stats){
        wr.set_file(C.outputdir + "/stats.depths_and_scores.txt");
    }
    build_model(G, reference, *C.cf, *C.slt_it, *C.rev_st_it, C.run_length_encoding, C.store_depths, wr);
    if(C.context_stats){ 
        write_context_summary(G, C.cf->get_number_of_candidates(), C.outputdir + "/stats.context_summary.txt");
    }
    write_log("Writing model to directory: " + C.outputdir);
    
    G.store_all_to_disk(C.outputdir, filename);
    C.write_to_file(C.outputdir, filename + ".info");
    
    return 0;
}

int main(int argc, char** argv){
    return build_model_main(argc, argv);
}

