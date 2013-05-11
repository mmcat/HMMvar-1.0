/*
 * blastSearch.cpp
 *
 *  Created on: Feb 16, 2013
 *      Author: mingmingliu
 */

#include <stdio.h>

#include "BlastSearch.h"

blastSearch::blastSearch(string blast_query,string blast_output,string subject_seqs,string blastout_save_name,string path){
	    blastout_save_file_name_ = blastout_save_name;
	    blast_query_file_name_ = blast_query;
	    blast_output_file_name_ = blast_output;
	    subject_sequences_file_name_ = subject_seqs;
	    temp_dir_ = path;
	    if(subject_sequences_file_name_.empty()){
	    		subject_sequences_file_name_ = temp_dir_+"/subject_file_tmp";
	    	}
}

blastSearch::~blastSearch(){

}

void blastSearch::searchHomoSeq(string blastdb_file_name,string psiblast_cmd){
	this->runBlast(blastdb_file_name,psiblast_cmd);
}

int blastSearch::runBlast(string blastdb_file_name,string psiblast_cmd) {

	int max_num = 100000;
	char blast_command[BUF_SIZE_MED];
	int return_code;

//	sprintf(blast_command, PSIBLAST );
	if(blast_output_file_name_.empty()){
		blast_output_file_name_ = temp_dir_+"\blastout_tmp";
	}
	if(blastdb_file_name.empty()) blastdb_file_name = BLASTDB;
	if(psiblast_cmd.empty()) psiblast_cmd = PSIBLAST;


	sprintf(blast_command, "%s -query %s -db %s -show_gis -max_target_seqs %d -num_iterations %d -outfmt \"6 sseqid sgi pident evalue bitscore \" -evalue %f -out %s",
			psiblast_cmd.c_str(),
			blast_query_file_name_.c_str(),
			blastdb_file_name.c_str(),
			max_num,
			5,
			0.01,
			blast_output_file_name_.c_str());

//	if (b_logging_) {
		Log("searching related sequences...\n", true);
//	}


	return_code = system(blast_command);

	if (return_code != 0) {
		fprintf(stderr, "psiblast failed (exit:%d)\n", return_code);
		exit(-1);
	}
	if (!blastout_save_file_name_.empty()) {
					char cp_command[BUF_SIZE_MED];
					sprintf(cp_command, "cp %s %s", blast_output_file_name_.c_str(), blastout_save_file_name_.c_str());
					// will replace system
					int ret;
					ret = system(cp_command);
					if (ret != 0) {
						fprintf(stderr, "saving blastout file failed (%d)\n", ret);
					}
				}


	return 0;
}

