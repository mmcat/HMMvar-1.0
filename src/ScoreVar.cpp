/*
 * scoreVar.cpp
 *
 *  Created on: Feb 17, 2013
 *      Author: Mingming Liu
 */

#include <string>
#include <stdio.h>
#include <math.h>
#include <algorithm>

#include "ScoreVar.h"
#include "CodeonAlphabeta.h"


using namespace std;

ScoreVar::ScoreVar(){

}

ScoreVar::~ScoreVar(){
	if (!tmp_dir_.empty()) {
			char rm_temp_dir_command[BUF_SIZE_MED];
			sprintf(rm_temp_dir_command, "rm -rf %s", tmp_dir_.c_str());
			int ret;
			ret = system(rm_temp_dir_command);
			if (ret != 0) {
				fprintf(stderr, "removing temporary directory failed (%d)\n", ret);
			}
		}

}



bool ScoreVar::SetQuerySequenceFromFastaFile(const char* id, string fasta_file){

	if(this->CreateTempDir()!=0) exit(-1);

	Log("loading query sequence from a FASTA file...\n", true);
	return query_seq_.SetSequenceFromFastaFile(id, fasta_file);
}

void ScoreVar::translate(const Sequence& nt, Sequence* aa,int type){
	aa->id_ = nt.id_;
	aa->def_ = nt.def_;
	string seqaa;
	string seqnt = nt.seq_;
	transform(seqnt.begin(),seqnt.end(),seqnt.begin(), ::tolower);
	replace(seqnt.begin(),seqnt.end(),'u','t');
	set_Codeon();
	for(int i=0; i<seqnt.length()-codeonsize+1;i+=codeonsize){
		string triplet = seqnt.substr(i,codeonsize);
		if(triplet.compare(codeongap)==0){
			seqaa+=gap;
		}
		else if(Codeon.find(triplet) != Codeon.end()){
			seqaa+=Tables[type-1].substr(Codeon[triplet],1);
		}
		else seqaa+='X';

	}
	unsigned stop = seqaa.find('*');
	aa->seq_ = seqaa.substr(0,stop);


}


int ScoreVar::getScore(string hmm_path,string wtaa_file_name,string multi_align_file){

	if(hmm_output_file_name_.empty()){
		hmm_output_file_name_ = "/hmm_out_tmp";
	}
	buildHMM(hmm_path,multi_align_file);
//	string query_seq_aa;
//	this->nt2aa(query_seq_.seq_, &query_seq_aa);
	double bs_wild = searchHMM(hmm_path,wtaa_file_name);
	int len = query_seq_.seq_.length()/3;
	double null_pro = getNullPro(len);
	double pro_wild = null_pro*exp(bs_wild*CONST_LOG2/INTSCALE);

	if (variants_.size()==0) {
		fprintf(stderr,"No variants found to be predicted!\n");
		exit(-1);
	}

	if(score_out_file_name_.empty()) score_out_file_name_ = "hmmVar.out";
	FILE* fp=fopen(score_out_file_name_.c_str(),"w");
	for(vector< variant >::iterator it = variants_.begin(); it!=variants_.end();it++){
		Sequence mut_seq_nt,mut_seq_aa;
		this->getMutantSeqFromVariants(*it,&mut_seq_nt);
		translate(mut_seq_nt,&mut_seq_aa,1);
		string mut_aa_file_name=tmp_dir_+"/query_aa_file_mt";
		mut_seq_aa.Print(mut_aa_file_name);
		double bs_mut = searchHMM(hmm_path,mut_aa_file_name);
		it->mt_proba_ = null_pro*exp(bs_mut*CONST_LOG2/INTSCALE);
		it->wt_proba_ = pro_wild;
		it->odds_ = getOdds(pro_wild,it->mt_proba_);
		fprintf(fp,"%s\t%d\t%.3f\n",it->varinat_str_.c_str(),it->pos,it->odds_);

	}

	fclose(fp);


	return 0;
}

int ScoreVar::getVariants(string filename){

	FILE* fp = fopen(filename.c_str(),"r");
	if(fp==NULL){printf("Cannot find variants file:No such file!\n");return -1;}
	char buffer[BUF_SIZE_MED];
	char *a;
	while(fgets(buffer,BUF_SIZE_MED,fp)!=NULL){
		variant var;
		a = strtok(buffer,"\t\n ");
		var.varinat_str_ = string(a);
		a = strtok(NULL,"\t\n ");
		var.pos = atof(a);
		var.variant_id_ = this->query_seq_.id_;
		variants_.push_back(var);
		}

	return 0;
}

double ScoreVar::getNullPro(int len){
	return exp(len*log(P1)+log(1.0-P1));
}

void ScoreVar::getMutantSeqFromVariants(variant var, Sequence* mut_seq){
	mut_seq->id_=query_seq_.id_;
	mut_seq->def_ = query_seq_.def_;
	mut_seq->seq_ = query_seq_.seq_;
	unsigned pos = var.varinat_str_.find('/');
	string w = var.varinat_str_.substr(0,pos);
	string m = var.varinat_str_.substr(pos+1);
	string s = query_seq_.seq_.substr(var.pos-1,w.size());
	if(w.compare(s)!=0){
		printf("Warning:The variant %s %d is not match with the sequence! Please check the variant.\n", var.varinat_str_.c_str(),var.pos);
	}
	if(m[0]=='_') mut_seq->seq_.erase(var.pos-1,w.size());
	else
		mut_seq->seq_.replace(var.pos-1,w.size(),m);

}



double ScoreVar::getOdds(double p1,double p2){
	if(p2==0 || p1==1) return 999;
	else return p1*(1-p2)/p2*(1-p1);

}
int ScoreVar::buildHMM(string hmm_path,string multi_align_file){
	char hmmbuild_cmd[BUF_SIZE_MED];
	string hmm_out_file = tmp_dir_+hmm_output_file_name_;
	if(hmm_path.empty()) hmm_path = HMMPATH;
	sprintf(hmmbuild_cmd,"%shmmbuild --amino --cpu 10 %s %s ",hmm_path.c_str(),hmm_out_file.c_str(),multi_align_file.c_str());

	Log("build hidden Markov model...\n",true);

	int return_code = system(hmmbuild_cmd);
		if(return_code!=0){
			fprintf(stderr, "hmmbuild failed (exit:%d)\n", return_code);
					exit(-1);

		}
		return 0;


}

double ScoreVar::searchHMM(string hmm_path,string aa_file_name){
	char hmmsearch_cmd[BUF_SIZE_MED];
	string hmm_out_file = tmp_dir_+hmm_output_file_name_;
	if(hmm_path.empty()) hmm_path = HMMPATH;
	sprintf(hmmsearch_cmd,"%shmmsearch %s %s|grep -A4 \"Scores for complete sequences\"|tail -n 1",hmm_path.c_str(), hmm_out_file.c_str(),aa_file_name.c_str());
	Log("maching hidden Markov model...\n",true);

	FILE *PIPE = popen(hmmsearch_cmd,"r");

	if(PIPE==NULL){
		fprintf(stderr, "hmmsearch failed (exit:%s)\n", hmmsearch_cmd);
		exit(-1);
	}

	char buf[BUF_SIZE_LARGE];
	char* str;
	fgets(buf,BUF_SIZE_MED,PIPE);
	str = strtok(buf, " \r\n");
	str = strtok(NULL," \r\n");
	if(str==NULL) return 0;
	return atof(str);
}

int ScoreVar::CreateTempDir(){
	char temp_dir[BUF_SIZE_MED];
		sprintf(temp_dir, "%s/hmmVarXXXXXX", P_tmpdir);

		if (mkdtemp(temp_dir) == NULL) {
			fprintf(stderr, "Error in creating temporary directory\n");
			return -1;
		}

		tmp_dir_ = temp_dir;

		return 0;
}

int ScoreVar::parseBlastResults(string blastdb_cmd,string blastdb,string subject_sequence_file_name,string blast_out_file_name,double cutoff){
	if (subject_sequence_file_name.empty()) return -1;


	char tmp_file_id[BUF_SIZE_MED];
	FILE* fp = fopen(blast_out_file_name.c_str(), "r");

	if (fp == NULL) {
		cerr << "cannot open: " << blast_out_file_name << endl;
		return -1;
	}

	char buf[BUF_SIZE_MED];

	char* db_id;
	char* gi;
	char* pident;
	int num_seqs = 0;

	sprintf(tmp_file_id, "%s/ids", this->tmp_dir_.c_str());
	FILE* fp_id_out = fopen(tmp_file_id, "w");

	char last_id[BUF_SIZE_MED] = "";

	// get subject seqs
	while (!feof(fp)) {
		if (fgets(buf, BUF_SIZE_MED, fp) == NULL)
			break;

		db_id = strtok(buf, "\t\n");
		gi = strtok(NULL, "\t\n");
		pident = strtok(NULL,"\t\n");

		char id[BUF_SIZE_MED]="";

		if (db_id == NULL || gi == NULL) {
			Log("Parsing Error\n", true);
			return -1;
		}

		strcpy(id, db_id);

		if (db_id != NULL && gi != NULL) {
			if (atof(pident)<cutoff*100 || strcmp(last_id, id) == 0) {
				continue;
			}

			fprintf(fp_id_out, "%s\n", id);
			strcpy(last_id, id);
			num_seqs++;
		} else {
			Log("Wrong entry in blast out\n", true);
			break;
		}
	}

	fclose(fp);
	fclose(fp_id_out);

	this->CreateFastaFileUsingBlastdbcmd(blastdb_cmd,blastdb,subject_sequence_file_name,tmp_file_id);

	return num_seqs;

}

int ScoreVar::CreateFastaFileUsingBlastdbcmd(string blastdb_cmd,string blastdb,string subject_seq_file_name,string file_id){
	char blastdbcmd[BUF_SIZE_MED];
	if(blastdb_cmd.empty()) blastdb_cmd = BLASTDBCMD;
	if(blastdb.empty()) blastdb = BLASTDB;
		sprintf(blastdbcmd, "%s -db %s -dbtype 'prot' -entry_batch %s > %s",
				blastdb_cmd.c_str(),
				blastdb.c_str(),
				file_id.c_str(),
				subject_seq_file_name.c_str()
				);
		system(blastdbcmd);

	return 0;
}

int ScoreVar::setHomoseq(string blastdb_cmd,string blastdb,string subject_sequence_file_name,string blast_out_file_name,double cutoff){


	Log("filtering related sequences...\n", true);

	return this->parseBlastResults(blastdb_cmd,blastdb,subject_sequence_file_name,blast_out_file_name,cutoff);

}
