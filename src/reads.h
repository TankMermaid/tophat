#ifndef READS_H
#define READS_H
/*
 *  reads.h
 *  TopHat
 *
 *  Created by Cole Trapnell on 9/2/08.
 *  Copyright 2008 Cole Trapnell. All rights reserved.
 *
 */

#include <string>
#include <sstream>
#include <queue>
#include <limits>
#include <boost/shared_ptr.hpp>
#include <seqan/sequence.h>
#include "common.h"
//#include "QReadData.h"


using std::string;





void reverse_complement(string& seq);
string str_convert_color_to_bp(const string& color);
seqan::String<char> convert_color_to_bp(char base, const seqan::String<char>& color);

string convert_bp_to_color(const string& bp, bool remove_primer = false);
seqan::String<char> convert_bp_to_color(const seqan::String<char>& bp, bool remove_primer = false);

/*
  This is a dynamic programming to decode a colorspace read, which is from BWA paper.

  Heng Li and Richard Durbin
  Fast and accurate short read alignment with Burrows-Wheeler transform
 */
void BWA_decode(const string& color, const string& qual, const string& ref, string& decode);


template <class Type>
string DnaString_to_string(const Type& dnaString)
{
  std::string result;
  std::stringstream ss(std::stringstream::in | std::stringstream::out);
  ss << dnaString >> result;
  return result;
}

class ReadTable;

class FLineReader { //simple text line reader class, buffering last line read
  int len;
  int allocated;
  char* buf;
  bool isEOF;
  FILE* file;
  bool is_pipe;
  bool pushed; //pushed back
  uint64_t lcount; //counting all lines read by the object
  
public:
  char* chars() { return buf; }
  char* line() { return buf; }
  uint64_t readcount() { return lcount; } //number of lines read
  int length() { return len; } //length of the last line read
  bool isEof() {return isEOF; }
  char* nextLine();
  FILE* fhandle() { return file; }
  void pushBack() { if (lcount) pushed=true; } // "undo" the last getLine request
           // so the next call will in fact return the same line
  FLineReader(FILE* stream=NULL) {
    len=0;
    isEOF=false;
    is_pipe=false;
    allocated=512;
    buf=(char*)malloc(allocated);
    lcount=0;
    buf[0]=0;
    file=stream;
    pushed=false;
    }
  FLineReader(FZPipe& fzpipe) {
    len=0;
    isEOF=false;
    allocated=512;
    buf=(char*)malloc(allocated);
    lcount=0;
    buf[0]=0;
    file=fzpipe.file;
    is_pipe=!fzpipe.pipecmd.empty();
    pushed=false;
    }

  void reset(FZPipe& fzpipe) {
    lcount=0;
    buf[0]=0;
    file=fzpipe.file;
    is_pipe=!fzpipe.pipecmd.empty();
    pushed=false;
    }

  void close() {
    if (file==NULL) return;
    if (is_pipe) pclose(file);
            else fclose(file);
    }

  ~FLineReader() {
    free(buf); //does not call close() -- we might reuse the file handle
    }
};



// Note: qualities are not currently used by TopHat
struct Read
{
	Read()
	{
		//seq.reserve(MAX_READ_LEN);
		//qual.reserve(MAX_READ_LEN);
	}

	string name;
	string seq;
	string alt_name;
	string qual;

	bool lengths_equal() { return seq.length() == qual.length(); }
	void clear()
	{
		name.clear();
		seq.clear();
		qual.clear();
		alt_name.clear();
	}
};


ReadFormat skip_lines(FLineReader& fr);
bool next_fastx_read(FLineReader& fr, Read& read, ReadFormat rd_format=FASTQ,
                        FLineReader* frq=NULL);

#define READSTREAM_BUF_SIZE 500000



struct QReadDataValue;
typedef boost::shared_ptr<QReadDataValue> QReadData;

struct QReadDataValue { //read data for the priority queue
  uint64_t id;
  Read read;
  char trashCode; //ZT tag value
  int8_t matenum; //mate number (1,2) 0 if unpaired
  bool um_written;

  static QReadData Null;

  QReadDataValue():id(0),read(),trashCode(0), matenum(0), um_written(false) { }
  QReadDataValue(uint64_t rid, const Read& rd, bam1_t* bd=NULL) {
  	  init(rid, rd, bd);
  }
  void init(uint64_t rid, const Read& rd, bam1_t* bd=NULL) {
	 id=rid;
	 read=rd;
	 trashCode=0;
	 matenum=0;
	 um_written=false;
     if (bd) {
       if (bd->core.flag & BAM_FREAD1) {
    	 matenum=1;
       }
       else if (bd->core.flag & BAM_FREAD2) matenum=2;
       GBamRecord bamrec(bd);
       trashCode=bamrec.tag_char("ZT");
     }
  }

  void clear() {
	  id=0;
	  read.clear();
	  trashCode=0;
	  matenum=0;
	  um_written=false;
  }

};


/*typedef struct  { //read data for the priority queue
	size_t refCount;
	uint64_t id;
	Read read;
	char trashCode; //ZT tag value
	int8_t matenum; //mate number (1,2) 0 if unpaired
	bool um_written;

	QReadDataValue():id(0),read(),trashCode(0), matenum(0), um_written(false),refCount(1) {}
	QReadDataValue(uint64_t rid, Read& rd, bam1_t* bd=NULL);
	~QReadDataValue();

	void clear();
			// void init(uint64_t rid, Read& rd, bam1_t* bd=NULL);
} QReadDataValue;*/





/*void deleteQReadDataValue() {
	// nothing to do for the moment. May be interesting later to log deletion.
}*/


//callback struct for ReadStream::getRead() - called for each read in the stream
struct GetReadProc {
	 GBamWriter* um_out; //skipped (unmapped) reads will be written here
	 int64_t* unmapped_counter;
	 int64_t* multimapped_counter;

	 int64_t* u_unmapped_counter;
	 int64_t* u_multimapped_counter;

	 //char um_code;
	GetReadProc(GBamWriter* bamw=NULL, int64_t* um_counter=NULL, int64_t* mm_counter=NULL,
			int64_t* u_um_counter=NULL, int64_t* u_mm_counter=NULL):
		um_out(bamw), unmapped_counter(um_counter), multimapped_counter(mm_counter),
		u_unmapped_counter(u_um_counter), u_multimapped_counter(u_mm_counter)
		{ }

	virtual bool process(QReadData& rdata, bool& found) { //, bool is_unmapped) {
	//should return True  - if it returns FALSE it will cause getRead() to abort
	//(stops looking for target readId in the stream) and to return false (="not found")
		return true;
	}
	void writeUnmapped(const QReadData& rdata) {
		if (rdata->um_written) return;
		string rname(rdata->read.alt_name);
		size_t slash_pos=rname.rfind('/');
		if (slash_pos!=string::npos && rname.length()-slash_pos<4)
			rname.resize(slash_pos);
		GBamRecord bamrec(rname.c_str(), -1, 0, false, rdata->read.seq.c_str(),
				NULL, rdata->read.qual.c_str());
		if (rdata->matenum) {
			bamrec.set_flag(BAM_FPAIRED);
			if (rdata->matenum==1) bamrec.set_flag(BAM_FREAD1);
			else bamrec.set_flag(BAM_FREAD2);
		}
		//if (found && um_code && !rdata->trashCode) {
		//rdata->trashCode=um_code;
		//}
		if (rdata->trashCode) {
			//multi-mapped reads did not really QC-fail
			//should also not be written to unmapped.bam
			bamrec.add_aux("ZT", 'A', 1, (uint8_t*)&rdata->trashCode);
			//if (rdata->trashCode!='M')
				//bamrec.set_flag(BAM_FQCFAIL); //to be excluded from further processing?
		}
		um_out->write(&bamrec);
		rdata->um_written=true;
	}

	virtual ~GetReadProc() { }
};


class ReadStream {
	FLineReader* flquals;
	FLineReader* flseqs;
	bool stream_copy;
	ReadFormat read_format; //should be guessed when opening the stream
	bam1_t* b;
	bool bam_alt_name; //from BAM files, look for alt_name tag to retrieve the original read name
	bool bam_ignoreQC; //from BAM files, ignore QC flag (return the next read even if it has QC fail)
  protected:
    struct ReadOrdering
    {
      bool operator()(QReadData& lhs, QReadData& rhs)
      {
        return (lhs->id > rhs->id);
      }
    };
    FZPipe fstream;
    FZPipe* fquals;
    size_t ReadBufSize;
    std::priority_queue< QReadData,
         std::vector<QReadData>,
         ReadOrdering > read_pq;
    uint64_t last_id; //keep track of last requested ID, for consistency check
    bool r_eof;
    bool next_read(QReadData& rdata); //get top read from the queue

  public:
    ReadStream(int bufsize=READSTREAM_BUF_SIZE):flquals(NULL), flseqs(NULL), stream_copy(false), read_format(FASTX_AUTO), b(NULL),
       bam_alt_name(false), bam_ignoreQC(false), fstream(), fquals(NULL),ReadBufSize(bufsize), read_pq(),
       last_id(0), r_eof(false) {   }

    ReadStream(const string& fname, FZPipe* pquals=NULL, bool guess_packer=false):flquals(NULL),
    	flseqs(NULL), stream_copy(false), read_format(FASTX_AUTO), b(NULL), bam_alt_name(false), bam_ignoreQC(false), fstream(),
    	fquals(pquals), ReadBufSize(READSTREAM_BUF_SIZE), read_pq(), last_id(0), r_eof(false) {
      init(fname, pquals, guess_packer);
    }
    ReadStream(FZPipe& f_stream, FZPipe* pquals=NULL):flquals(NULL),
    	 flseqs(NULL), stream_copy(true), read_format(FASTX_AUTO), b(NULL), bam_alt_name(false), bam_ignoreQC(false), fstream(f_stream),
    	 fquals(pquals), ReadBufSize(READSTREAM_BUF_SIZE), read_pq(), last_id(0), r_eof(false) {
      //init(f_stream, pquals);
      if (fstream.is_bam) {
        b = bam_init1();
      }
      else  {
        flseqs=new FLineReader(fstream.file);
        read_format=skip_lines(*flseqs);
        if (read_format==FASTX_AUTO)
          warn_msg("Warning: could not determine format for file '%s'! (large header?)\n", fstream.filename.c_str());

      }
      fquals=pquals;
      if (fquals) {
        flquals=new FLineReader(fquals->file);
        skip_lines(*flquals);
        }
    }
    void use_alt_name(bool v=true) {
      bam_alt_name=v;
    }

    void ignoreQC(bool v=true) {
      bam_ignoreQC=v;
    }
    ReadFormat get_format() { return read_format; }
    void init(const string& fname, FZPipe* pquals=NULL, bool guess_packer=false) {
       if (fname.empty()) return;
       if (fstream.openRead(fname, guess_packer)==NULL) {
          fprintf(stderr, "Warning: couldn't open file %s\n",fname.c_str());
          return;
       }
       if (fstream.is_bam) {
          if (b==NULL) {
        	b = bam_init1();
           }
       }
       else  {
          if (b) { bam_destroy1(b); b=NULL; }
          flseqs=new FLineReader(fstream.file);
          read_format=skip_lines(*flseqs);
          if (read_format==FASTX_AUTO)
            warn_msg("Warning: could not determine format for file '%s'! (large header?)\n", fstream.filename.c_str());

       }
       fquals=pquals;
       if (fquals) {
          flquals=new FLineReader(fquals->file);
          skip_lines(*flquals);
       }
    }

    void init(FZPipe& f_stream, FZPipe* pquals=NULL) {
        fstream=f_stream; //Warning - original copy may end up with invalid (closed) file handle
        stream_copy=true;
        if (fstream.file==NULL) {
          fprintf(stderr, "Warning: ReadStream not open.\n");
          return;
        }
        if (fstream.is_bam) {
          if (b==NULL) {
            b = bam_init1();
            }
        }
        else  {
          if (b) { bam_destroy1(b); b=NULL; }
          flseqs=new FLineReader(fstream.file);
          read_format=skip_lines(*flseqs);
          if (read_format==FASTX_AUTO)
            warn_msg("Warning: could not determine format for file '%s'! (large header?)\n", f_stream.filename.c_str());
        }
        fquals=pquals;
        if (fquals) {
          flquals=new FLineReader(fquals->file);
          skip_lines(*flquals);
        }
    }

    //unbuffered reading from stream
    bool get_direct(Read& read);
    bool isBam() { return fstream.is_bam; }
    bam1_t* last_b() {//return the latest SAM record data fetched by get_direct()
      //must only be called after get_direct()
      return b;
      }

    const char* filename() {
        return fstream.filename.c_str();
        }

    //read_ids must ALWAYS be requested in increasing order
    bool getRead(uint64_t read_id, Read& read,
		 //ReadFormat read_format=FASTQ,
		 bool strip_slash=false,
		 uint64_t begin_id = 0,
		 uint64_t end_id=std::numeric_limits<uint64_t>::max(),
		 GetReadProc* rProc=NULL,
		 bool is_unmapped=false //the target read, when found is also written by
		                        //rProc into the unmapped BAM file
		 /*
		 GBamWriter* um_out=NULL, //skipped (unmapped) reads will be written here
		 // char um_code=0
		 int64_t* unmapped_counter=NULL,
		 int64_t* multimapped_counter=NULL
		 */
		 );
    bool getQRead(uint64_t read_id,
    		QReadData& read,
    		uint64_t begin_id = 0,
    		uint64_t end_id=std::numeric_limits<uint64_t>::max(),
    		GetReadProc* rProc=NULL,
    		bool is_unmapped=false //the target read, when found is also written by
		                        //rProc into the unmapped BAM file
		    );


    void rewind() {
      fstream.rewind();
      r_eof=false;
      clear();
      if (flseqs) {
        flseqs->reset(fstream);
        skip_lines(*flseqs); //read_format detected earlier
        }
      if (flquals) {
    	flquals->reset(*fquals);
        }
      }

    void seek(int64_t offset) {
      clear();
      fstream.seek(offset);
    }

    FILE* file() {
      return fstream.file;
    }

    void clear() {
      read_pq=std::priority_queue< QReadData,
          std::vector<QReadData>,
          ReadOrdering > ();
      last_id=0;
    }

    void close() {
      clear();
      fstream.close();
    }
    ~ReadStream() {
      close();
      if (b) { bam_destroy1(b); }
      if (flquals) delete flquals;
      if (flseqs) delete flseqs;
    }
};
#endif
