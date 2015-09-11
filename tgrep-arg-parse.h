#include <getopt.h>
 void usage(FILE* stream, int exit_code);

#define FREE_IOVEC_QUEUE_SIZE 8192
#define MAX_SEARCH_TERM_LEN 1024
#define IOVEC_BLOCK_SIZE 4096
#define MAX_FILE_NAME 2048

enum path_type
{
	path_type_dir,path_type_file,path_type_stdin
}
;
struct config
{
	char*  search_term;
	char*  read_from;
	int  num_readers;
	int  num_searchers;
	int  debug;
	int  iovec_block_size;
	int  iovec_queue_size;
	enum path_type path_type;
}
;
void config_init(struct config* cfg) 
{
	cfg->search_term = NULL;
	cfg->read_from = NULL;
	cfg->num_readers = 1;
	cfg->num_searchers = 1;
	cfg->debug = 0;
	cfg->iovec_block_size = IOVEC_BLOCK_SIZE;
	cfg->iovec_queue_size = FREE_IOVEC_QUEUE_SIZE;
	cfg->path_type = path_type_file;
}
struct config cfg;
struct config* parse_args(int argc, char* argv[], struct config* cfg)
{
	config_init(cfg);
	extern int optind;
	extern char* optarg;
	int next_opt = 0;
	const char* short_options = "r:s:D:b:q:";
	const struct option long_options[] = {{"num_readers",1,NULL,'r'},{"num_searchers",1,NULL,'s'},{"debug-level",1,NULL,'D'},{"block-size",1,NULL,'b'},{"queue-size",1,NULL,'q'}};
	do
	{
		next_opt = getopt_long(argc,argv,short_options,long_options,NULL);
		switch(next_opt) { 
		case 'r':
			{
				cfg->num_readers = atoi(optarg) ;
				break;
			}
		case 's':
			{
				cfg->num_searchers = atoi(optarg) ;
				break;
			}
		case 'D':
			{
				cfg->debug = atoi(optarg) ;
				break;
			}
		case 'b':
			{
				cfg->iovec_block_size = atoi(optarg) ;
				break;
			}
		case 'q':
			{
				cfg->iovec_queue_size = atoi(optarg) ;
				break;
			}
		case '?':
			{
				usage(stderr,-1);
				break;
			}
		case -1:
			{
				break;
			}
		default:
			{
				printf("unexpected exit ");
				abort();
			}
		};
	}
	 while (next_opt != -1);
	int remaining_args = argc - optind;
	if (remaining_args < 2) 
	{
		fprintf(stderr, "Insufficient number of args 2 args required  \n");
		exit(-1);
	}
	cfg->search_term = argv[optind+0];
	cfg->read_from = argv[optind+1];
	return cfg;
}
void usage(FILE* stream, int exit_code)
{
	fprintf(stream,"Usage: tgrep  <options> [input file] \n");
	struct description
	{
		char* short_option;
		char* long_option;
		char* description;
	}
	;
	const struct description desc_arr[] = {{"r","--num_readers","Number of readers to run simultaneously"},{"s","--num_searchers","Number of searchers to run simultaneously"},{"D","--debug-level","Debug verbosity"},{"b","--block-size","Block size for debug queue"},{"q","--queue-size","Queue for debug queue"}};
	int i = 0;
	for(i = 0; i < 4 ; i++)
	{
		fprintf(stream,"%-10s%-14s\t%-10s\n",desc_arr[i].short_option,desc_arr[i].long_option,desc_arr[i].description);
	}
	exit(exit_code);
}
