#ifndef BUILD_MODEL_HH
#define BUILD_MODEL_HH

#include "BPR_tools.hh"
#include "Parent_Support.hh"
#include "LMA_Support.hh"
#include "String_Depth_Support.hh"
#include "globals.hh"
#include "BWT_iteration.hh"
#include "Precalc.hh"
#include "context_marking.hh"
#include "Interfaces.hh"
#include "BPR_Colex_mapping.hh"
#include "UniBWT.h"
#include "RLEBWT.hh"
#include "Basic_bitvector.hh"
#include "RLE_bitvector.hh"
#include "logging.hh"
#include "build_model.hh"
#include <stack>
#include <vector>
#include <memory>

bool is_all_ones(sdsl::bit_vector& v){
    for(int64_t i = 0; i < v.size(); i++){
        if(v[i] == 0) return false;
    }
    return true;
}

// All components of the model will be stored into G
// T: reference string
// context_formula: a callback for context marking
// slt_it: iterator that gives all nodes that we want in the SLT
// rev_st_it: iterator that gives all nodes that we want in the rev ST
// run_length_coding: self-explatonary
// compute_string_depths: Get precomputed string depths
// wr: where to write context stats

void build_model(Global_Data& G, string& T, Context_Callback& context_formula,
                 Iterator& slt_it, Iterator& rev_st_it, bool run_length_coding, bool compute_string_depths, Stats_writer& wr){
        
    
    write_log("Building the BiBWT");
    G.bibwt = make_shared<BD_BWT_index<>>((uint8_t*)T.c_str());
    
    slt_it.set_index(G.bibwt.get());
    rev_st_it.set_index(G.bibwt.get());
    
    write_log("Building reverse suffix tree BPR and pruning marks");
    Build_REV_ST_BPR_And_Pruning_Callback revstbprcb;
    revstbprcb.init(*G.bibwt);
    iterate_with_callbacks(rev_st_it, &revstbprcb);
    Rev_st_topology RSTT = revstbprcb.get_result();
    G.rev_st_bpr = std::shared_ptr<Bitvector>(new Basic_bitvector(RSTT.bpr));
    
    G.rev_st_bpr->init_rank_10_support();
    G.rev_st_bpr->init_select_10_support();
    G.rev_st_bpr->init_bps_support();
    
    if(is_all_ones(RSTT.pruning_marks)){
        write_log("Pruning marks is all ones");
        G.pruning_marks = make_shared<All_Ones_Bitvector>(RSTT.pruning_marks.size());
    } else{
        if(run_length_coding){
            write_log("Run length coding the pruning marks vector");
            G.pruning_marks = std::shared_ptr<Bitvector>(new RLE_bitvector(RSTT.pruning_marks));
        } else{
            G.pruning_marks = std::shared_ptr<Bitvector>(new Basic_bitvector(RSTT.pruning_marks));
        }
    }
    
    G.pruning_marks->init_rank_support();
    G.pruning_marks->init_select_support();
    
    Pruned_Topology_Mapper mapper(G.rev_st_bpr, G.pruning_marks);
    
    sdsl::bit_vector sdsl_slt_bpr; // Assigned to later if compute_string_depths is false
    
    if(!compute_string_depths) write_log("Building SLT BPR");
    Build_SLT_BPR_Callback sltbprcb;
    
    if(compute_string_depths) sltbprcb.disable();
    else sltbprcb.enable();
    
    sltbprcb.init(*G.bibwt);
    iterate_with_callbacks(slt_it, &sltbprcb);
    sdsl_slt_bpr = sltbprcb.get_result(); // Returns empty if disabled
    
    if(run_length_coding){
        if(!compute_string_depths) write_log("Run length coding SLT BPR");
        G.slt_bpr = make_shared<RLE_bitvector>(sdsl_slt_bpr);
    } else{
        G.slt_bpr = make_shared<Basic_bitvector>(sdsl_slt_bpr);
    }
    
    G.slt_bpr->init_rank_support();
    
    wr.set_data(&G);
    if(compute_string_depths) write_log("Marking contexts and storing string depths of maxreps");
    else write_log("Marking contexts and maxreps");
    
    Rev_ST_Maximal_Marks_Callback revstmmcb;
    SLT_Maximal_Marks_Callback sltmmcb;
    Store_Depths_Callback sdcb;
    
    if(compute_string_depths) {
        sdcb.enable();
        sltmmcb.disable();
    } else {
        sdcb.disable();
        sltmmcb.enable();
    }
    
    revstmmcb.init(*G.bibwt, G.rev_st_bpr->size(), mapper);
    sltmmcb.init(*G.bibwt, sdsl_slt_bpr);
    context_formula.init(G.bibwt.get(), G.rev_st_bpr->size(), mapper, &wr);
    sdcb.init();
    
    vector<Iterator_Callback*> marking_callbacks = {&sdcb, &revstmmcb, &sltmmcb, &context_formula};
    iterate_with_callbacks(slt_it, marking_callbacks);
    
    G.rev_st_maximal_marks = std::shared_ptr<Bitvector>(new Basic_bitvector(revstmmcb.get_result()));
    G.rev_st_maximal_marks->init_rank_support();
    
    G.rev_st_context_marks = std::shared_ptr<Bitvector>(new Basic_bitvector(context_formula.get_result()));
    G.rev_st_context_marks->init_rank_support();
    G.rev_st_context_marks->init_select_support();
    
    G.string_depths = std::shared_ptr<sdsl::int_vector<0>>(new sdsl::int_vector<0>(sdcb.get_result()));

    sdsl::bit_vector sdsl_slt_maxreps = sltmmcb.get_result();
    if(run_length_coding){
        if(!compute_string_depths) write_log("Run length coding maximal repeats on SLT BPR");
        G.slt_maximal_marks = std::shared_ptr<Bitvector>(new RLE_bitvector(sdsl_slt_maxreps));
    } else{
        G.slt_maximal_marks = std::shared_ptr<Bitvector>(new Basic_bitvector(sdsl_slt_maxreps));
    }
    G.slt_maximal_marks->init_rank_support();
    G.slt_maximal_marks->init_select_support();
    
    write_log("Building the BPR of contexts only");
    G.rev_st_bpr_context_only = std::shared_ptr<Bitvector>(new Basic_bitvector(get_rev_st_bpr_context_only(&G)));
    G.rev_st_bpr_context_only->init_bps_support();
    
    // Store reverse BWT for scoring. Todo: reuse already computed bibwt
    
    uint8_t* revbwt = (uint8_t*)malloc(sizeof(uint8_t) * T.size()+1+1); // +1: dollar, +1: null terminator
    for(int64_t i = 0; i < T.size()+1; i++){
        revbwt[i] = G.bibwt->backward_bwt_at(i);
    }
    revbwt[T.size()+1] = 0;
    
    if(run_length_coding){
        write_log("Run length coding the reverse BWT");
        G.revbwt = make_shared<RLEBWT<>>();
        G.revbwt->init_from_bwt(revbwt);
    } else{
        write_log("Storing the reverse BWT");
        G.revbwt = make_shared<Basic_BWT<>>();
        G.revbwt->init_from_bwt(revbwt);
    }
    
    free(revbwt);
    
    //cout << G.toString() << endl;
        
}

void build_model(Global_Data& G, string& T, Context_Callback& context_formula,
                 Iterator& slt_it, Iterator& rev_st_it, bool run_length_coding, bool compute_string_depths){
    // No score Scores_writer
    Stats_writer wr;
    build_model(G,T,context_formula, slt_it, rev_st_it, run_length_coding, compute_string_depths, wr);
}

int build_model_main(int argc, char** argv);

/*
void write_depth_statistics(Global_Data& G, string filepath){
    ofstream depths;
    depths.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    depths.open(filepath);
    Pruned_Topology_Mapper mapper(G.rev_st_bpr, G.pruning_marks); // Works for both pruned and non-pruned
    String_Depth_Support SDS;
    init_support<String_Depth_Support>(SDS, &G);
    
    depths << "string-depth rev-st-treedepth" << endl;
    for(int64_t i = 0; i < G.rev_st_context_marks->size(); i++){
        if(G.rev_st_context_marks->at(i) == 1 && G.rev_st_bpr->at(i) == 1){
            // Open parenthesis of a marked node
            // Compute string depth
            if(G.rev_st_maximal_marks->at(i) == 1){
                // Maxrep
                depths << SDS.string_depth(i) << " ";
            } else{
                // One-char left-extension of a maxrep. Go to parent and add 1 to depth
                depths << SDS.string_depth(G.rev_st_bpr->enclose(i)) + 1 << " ";
            }
            
            // Compute tree depth
            depths << G.rev_st_bpr->excess(i) - 1; //-1: root has depth 0
            depths << "\n";
        }
    }
} */

void write_context_summary(Global_Data& G, int64_t n_candidates, string filepath){
    ofstream summary;
    summary.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    summary.open(filepath);
    summary << "Number of contexts: " << G.rev_st_bpr_context_only->size() / 2 << endl; // Open and close marked for each context
    summary << "Number of context candidates: " << n_candidates << endl; // Open and close marked for each
}




#endif
