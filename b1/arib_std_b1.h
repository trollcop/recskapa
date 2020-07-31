#ifndef ARIB_STD_B1_H
#define ARIB_STD_B1_H

#include "portable.h"
#include "b_cas_card.h"

typedef struct {
	uint8_t *data;
	int32_t  size;
} ARIB_STD_B1_BUFFER;

typedef struct {

	int32_t  program_number; /* channel */

	int32_t  ecm_unpurchased_count;
	int32_t  last_ecm_error_code;

	int32_t  padding;

	int64_t  total_packet_count;
	int64_t  undecrypted_packet_count;

} ARIB_STD_B1_PROGRAM_INFO;

typedef struct {

	void *private_data;

	void (* release)(void *std_b1);

	int (* set_multi2_round)(void *std_b1, int32_t round);
	int (* set_strip)(void *std_b1, int32_t strip);

	int (* set_b_cas_card)(void *std_b1, B_CAS_CARD *bcas);

	int (* reset)(void *std_b1);
	int (* flush)(void *std_b1);

	int (* put)(void *std_b1, ARIB_STD_B1_BUFFER *buf);
	int (* get)(void *std_b1, ARIB_STD_B1_BUFFER *buf);

	int (* get_program_count)(void *std_b1);
	int (* get_program_info)(void *std_b1, ARIB_STD_B1_PROGRAM_INFO *info, int32_t idx);

} ARIB_STD_B1;

#ifdef __cplusplus
extern "C" {
#endif

extern ARIB_STD_B1 *create_arib_std_b1();

#ifdef __cplusplus
}
#endif

#endif /* ARIB_STD_B1_H */

