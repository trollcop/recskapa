#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(WIN32)
	#include <io.h>
	#include <windows.h>
	#include <crtdbg.h>
#else
	#define __STDC_FORMAT_MACROS
	#include <inttypes.h>
	#include <unistd.h>
        #include <errno.h>
	#include <pthread.h>
	#include <sys/poll.h>
	#include <linux/futex.h>
	#include <sys/time.h>
	#include <sys/resource.h>
	#include <signal.h>
#endif

#include "arib_std_b25.h"
#include "b_cas_card.h"

#ifdef _WAKE_BROADCAST
#define wake_thread(a)  pthread_cond_broadcast(a)
#else
#define wake_thread(a)  pthread_cond_signal(a)
#endif

typedef struct {
	int32_t round;
	int32_t strip;
	int32_t emm;
	int32_t verbose;
	int32_t power_ctrl;
} OPTION;

//static void show_usage();
static int parse_arg(OPTION *dst, int argc, char **argv);
static void test_arib_std_b25(OPTION *opt);
static void show_bcas_power_on_control_info(B_CAS_CARD *bcas);

int main(int argc, char **argv)
{
	int n;
	OPTION opt;
	
	#if defined(WIN32)
	_CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE );
	_CrtSetReportFile( _CRT_WARN, _CRTDBG_FILE_STDOUT );
	_CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_FILE );
	_CrtSetReportFile( _CRT_ERROR, _CRTDBG_FILE_STDOUT );
	_CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE );
	_CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDOUT );
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|_CRTDBG_DELAY_FREE_MEM_DF|_CRTDBG_CHECK_ALWAYS_DF|_CRTDBG_LEAK_CHECK_DF);
	#endif
	
	n = parse_arg(&opt, argc, argv);

	test_arib_std_b25(&opt);

	#if defined(WIN32)
	_CrtDumpMemoryLeaks();
	#endif

	return EXIT_SUCCESS;
}

/* static void show_usage()
{
	fprintf(stderr, "b1 - ARIB STD-B1 test program ver. 0.0 (2012, 9/14)\n");
	fprintf(stderr, "usage: b1 [options] src.m2t dst.m2t\n");
	fprintf(stderr, "options:\n");
	fprintf(stderr, "  -r round (integer, default=4)\n");
	fprintf(stderr, "  -s strip\n");
	fprintf(stderr, "     0: keep null(padding) stream (default)\n");
	fprintf(stderr, "     1: strip null stream\n");
	// EMM,通電指示情報は未サポート
	//fprintf(stderr, "  -m EMM\n");
	//fprintf(stderr, "     0: ignore EMM (default)\n");
	//fprintf(stderr, "     1: send EMM to B-CAS card\n");
	//fprintf(stderr, "  -p power_on_control_info\n");
	//fprintf(stderr, "     0: do nothing additionaly\n");
	//fprintf(stderr, "     1: show B-CAS EMM receiving request (default)\n");
	fprintf(stderr, "  -v verbose\n");
	fprintf(stderr, "     0: silent\n");
	fprintf(stderr, "     1: show processing status (default)\n");
	fprintf(stderr, "\n");
} */

static int parse_arg(OPTION *dst, int argc, char **argv)
{
	int i;
	
	dst->round = 4;
	dst->strip = 1;
	// EMM,通電指示情報は未サポート
	dst->emm = 0;
	//dst->power_ctrl = 1;
	dst->power_ctrl = 0;
	dst->verbose = 0;

	for(i=1;i<argc;i++){
		if(argv[i][0] != '-'){
			break;
		}
		switch(argv[i][1]){
		// EMM,通電指示情報は未サポート
		//case 'm':
		//	if(argv[i][2]){
		//		dst->emm = atoi(argv[i]+2);
		//	}else{
		//		dst->emm = atoi(argv[i+1]);
		//		i += 1;
		//	}
		//	break;
		//case 'p':
		//	if(argv[i][2]){
		//		dst->power_ctrl = atoi(argv[i]+2);
		//	}else{
		//		dst->power_ctrl = atoi(argv[i+1]);
		//		i += 1;
		//	}
		//	break;
		case 'r':
			if(argv[i][2]){
				dst->round = atoi(argv[i]+2);
			}else{
				dst->round = atoi(argv[i+1]);
				i += 1;
			}
			break;
		case 's':
			if(argv[i][2]){
				dst->strip = atoi(argv[i]+2);
			}else{
				dst->strip = atoi(argv[i+1]);
				i += 1;
			}
			break;
		case 'v':
			if(argv[i][2]){
				dst->verbose = atoi(argv[i]+2);
			}else{
				dst->verbose = atoi(argv[i+1]);
				i += 1;
			}
			break;
		default:
			fprintf(stderr, "error - unknown option '-%c'\n", argv[i][1]);
			return argc;
		}
	}

	return i;
}

static void test_arib_std_b25(OPTION *opt)
{
	int code,i,n,m;
	int sfd,dfd;

	int64_t total;
	int64_t offset;

	ARIB_STD_B25 *b25;
	B_CAS_CARD   *bcas;

	ARIB_STD_B25_PROGRAM_INFO pgrm;

	uint8_t data[8*1024];

	ARIB_STD_B25_BUFFER sbuf;
	ARIB_STD_B25_BUFFER dbuf;

	sfd = 0;
	dfd = 1;
	b25 = NULL;
	bcas = NULL;
	
	_lseeki64(sfd, 0, SEEK_END);
	total = _telli64(sfd);
	_lseeki64(sfd, 0, SEEK_SET);

	b25 = create_arib_std_b25();
	if(b25 == NULL){
		fprintf(stderr, "error - failed on create_arib_std_b25()\n");
		goto LAST;
	}

	code = b25->set_multi2_round(b25, opt->round);
	if(code < 0){
		fprintf(stderr, "error - failed on ARIB_STD_B25::set_multi2_round() : code=%d\n", code);
		goto LAST;
	}

	code = b25->set_strip(b25, opt->strip);
	if(code < 0){
		fprintf(stderr, "error - failed on ARIB_STD_B25::set_strip() : code=%d\n", code);
		goto LAST;
	}

	code = b25->set_emm_proc(b25, opt->emm);
	if(code < 0){
		fprintf(stderr, "error - failed on ARIB_STD_B25::set_emm_proc() : code=%d\n", code);
		goto LAST;
	}

	bcas = create_b_cas_card();
	if(bcas == NULL){
		fprintf(stderr, "error - failed on create_b_cas_card()\n");
		goto LAST;
	}

	code = bcas->init(bcas);
	if(code < 0){
		fprintf(stderr, "error - failed on B_CAS_CARD::init() : code=%d\n", code);
		goto LAST;
	}

	code = b25->set_b_cas_card(b25, bcas);
	if(code < 0){
		fprintf(stderr, "error - failed on ARIB_STD_B25::set_b_cas_card() : code=%d\n", code);
		goto LAST;
	}

	offset = 0;
	while( (n = _read(sfd, data, sizeof(data))) > 0 ){
		sbuf.data = data;
		sbuf.size = n;

		code = b25->put(b25, &sbuf);
		if(code < 0){
			fprintf(stderr, "error - failed on ARIB_STD_B25::put() : code=%d\n", code);
			if (code != -6) {
				goto LAST;
			}
		}

		code = b25->get(b25, &dbuf);
		if(code < 0){
			fprintf(stderr, "error - failed on ARIB_STD_B25::get() : code=%d\n", code);
			goto LAST;
		}

		if(dbuf.size > 0){
			n = _write(dfd, dbuf.data, dbuf.size);
			if(n != dbuf.size){
				fprintf(stderr, "error failed on _write(%d)\n", dbuf.size);
				goto LAST;
			}
		}
		
		offset += sbuf.size;
		if(opt->verbose != 0){
			m = (int)(10000*offset/total);
			fprintf(stderr, "\rprocessing: %2d.%02d%% ", m/100, m%100);
		}
	}

	code = b25->flush(b25);
	if(code < 0){
		fprintf(stderr, "error - failed on ARIB_STD_B25::flush() : code=%d\n", code);
		goto LAST;
	}
	
	code = b25->get(b25, &dbuf);
	if(code < 0){
		fprintf(stderr, "error - failed on ARIB_STD_B25::get() : code=%d\n", code);
		goto LAST;
	}

	if(dbuf.size > 0){
		n = _write(dfd, dbuf.data, dbuf.size);
		if(n != dbuf.size){
			fprintf(stderr, "error - failed on _write(%d)\n", dbuf.size);
			goto LAST;
		}
	}

	if(opt->verbose != 0){
		fprintf(stderr, "\rprocessing: finish  \n");
		fflush(stderr);
		fflush(stdout);
	}

	n = b25->get_program_count(b25);
	if(n < 0){
		fprintf(stderr, "error - failed on ARIB_STD_B25::get_program_count() : code=%d\n", code);
		goto LAST;
	}
	for(i=0;i<n;i++){
		code = b25->get_program_info(b25, &pgrm, i);
		if(code < 0){
			fprintf(stderr, "error - failed on ARIB_STD_B25::get_program_info(%d) : code=%d\n", i, code);
			goto LAST;
		}
		if(pgrm.ecm_unpurchased_count > 0){
			fprintf(stderr, "warning - unpurchased ECM is detected\n");
			fprintf(stderr, "  channel:               %d\n", pgrm.program_number);
			fprintf(stderr, "  unpurchased ECM count: %d\n", pgrm.ecm_unpurchased_count);
			fprintf(stderr, "  last ECM error code:   %04x\n", pgrm.last_ecm_error_code);
			fprintf(stderr, "  undecrypted TS packet: %d\n", pgrm.undecrypted_packet_count);
			fprintf(stderr, "  total TS packet:       %d\n", pgrm.total_packet_count);
		}
	}

	if(opt->power_ctrl != 0){
		show_bcas_power_on_control_info(bcas);
	}

LAST:

	if(sfd >= 0){
		_close(sfd);
		sfd = -1;
	}

	if(dfd >= 0){
		_close(dfd);
		dfd = -1;
	}

	if(b25 != NULL){
		b25->release(b25);
		b25 = NULL;
	}

	if(bcas != NULL){
		bcas->release(bcas);
		bcas = NULL;
	}
}

static void show_bcas_power_on_control_info(B_CAS_CARD *bcas)
{
	int code;
	int i,w;
	B_CAS_PWR_ON_CTRL_INFO pwc;

	code = bcas->get_pwr_on_ctrl(bcas, &pwc);
	if(code < 0){
		fprintf(stderr, "error - failed on B_CAS_CARD::get_pwr_on_ctrl() : code=%d\n", code);
		return;
	}

	if(pwc.count == 0){
		fprintf(stderr, "no EMM receiving request\n");
		return;
	}

	fprintf(stderr, "total %d EMM receiving request\n", pwc.count);
	for(i=0;i<pwc.count;i++){
		fprintf(stderr, "+ [%d] : tune ", i);
		switch(pwc.data[i].network_id){
		case 4:
			w = pwc.data[i].transport_id;
			fprintf(stderr, "BS-%d/TS-%d ", ((w >> 4) & 0x1f), (w & 7));
			break;
		case 6:
		case 7:
			w = pwc.data[i].transport_id;
			fprintf(stderr, "ND-%d/TS-%d ", ((w >> 4) & 0x1f), (w & 7));
			break;
		default:
			fprintf(stderr, "unknown(b:0x%02x,n:0x%04x,t:0x%04x) ", pwc.data[i].broadcaster_group_id, pwc.data[i].network_id, pwc.data[i].transport_id);
			break;
		}
		fprintf(stderr, "between %04d %02d/%02d ", pwc.data[i].s_yy, pwc.data[i].s_mm, pwc.data[i].s_dd);
		fprintf(stderr, "to %04d %02d/%02d ", pwc.data[i].l_yy, pwc.data[i].l_mm, pwc.data[i].l_dd);
		fprintf(stderr, "least %d hours\n", pwc.data[i].hold_time);
	}
}

