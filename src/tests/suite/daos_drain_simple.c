/**
 * (C) Copyright 2016-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
/**
 * This file is for simple tests of drain, which does not need to kill the
 * rank, and only used to verify the consistency after different data model
 * drains.
 *
 * tests/suite/daos_drain_simple.c
 *
 *
 */
#define D_LOGFAC	DD_FAC(tests)

#include "daos_iotest.h"
#include <daos/pool.h>
#include <daos/mgmt.h>
#include <daos/container.h>

#define KEY_NR		100
#define OBJ_NR		10
#define OBJ_CLS		OC_RP_3G1
#define OBJ_REPLICAS	3
#define DEFAULT_FAIL_TGT 0
#define DRAIN_POOL_SIZE	(4ULL << 30)
#define DRAIN_SUBTEST_POOL_SIZE (1ULL << 30)
#define DRAIN_SMALL_POOL_SIZE (1ULL << 28)

static void
drain_dkeys(void **state)
{
	test_arg_t		*arg = *state;
	daos_obj_id_t		oid;
	struct ioreq		req;
	int			tgt = DEFAULT_FAIL_TGT;
	int			i;

	if (!test_runable(arg, 4))
		return;

	oid = daos_test_oid_gen(arg->coh, DAOS_OC_R1S_SPEC_RANK, 0, 0,
				arg->myrank);
	oid = dts_oid_set_rank(oid, ranks_to_kill[0]);
	oid = dts_oid_set_tgt(oid, tgt);
	ioreq_init(&req, arg->coh, oid, DAOS_IOD_ARRAY, arg);

	/** Insert 1000 records */
	print_message("Insert %d kv record in object "DF_OID"\n",
		      KEY_NR, DP_OID(oid));
	for (i = 0; i < KEY_NR; i++) {
		char	key[32] = {0};

		sprintf(key, "dkey_0_%d", i);
		insert_single(key, "a_key", 0, "data", strlen("data") + 1,
			      DAOS_TX_NONE, &req);
	}

	arg->rebuild_cb = reintegrate_inflight_io;
	arg->rebuild_cb_arg = &oid;
	drain_single_pool_target(arg, ranks_to_kill[0], tgt, false);

	for (i = 0; i < KEY_NR; i++) {
		char key[32] = {0};
		char buf[16] = {0};

		sprintf(key, "dkey_0_%d", i);
		/** Lookup */
		memset(buf, 0, 10);
		lookup_single(key, "a_key", 0, buf, 10, DAOS_TX_NONE, &req);
		assert_int_equal(req.iod[0].iod_size, strlen("data") + 1);

		/** Verify data consistency */
		assert_string_equal(buf, "data");
	}

	reintegrate_inflight_io_verify(arg);
	ioreq_fini(&req);
}

static void
drain_akeys(void **state)
{
	test_arg_t		*arg = *state;
	daos_obj_id_t		oid;
	struct ioreq		req;
	int			tgt = DEFAULT_FAIL_TGT;
	int			i;

	if (!test_runable(arg, 4))
		return;

	oid = daos_test_oid_gen(arg->coh, DAOS_OC_R1S_SPEC_RANK, 0, 0,
				arg->myrank);
	oid = dts_oid_set_rank(oid, ranks_to_kill[0]);
	oid = dts_oid_set_tgt(oid, tgt);
	ioreq_init(&req, arg->coh, oid, DAOS_IOD_ARRAY, arg);

	/** Insert 1000 records */
	print_message("Insert %d kv record in object "DF_OID"\n",
		      KEY_NR, DP_OID(oid));
	for (i = 0; i < KEY_NR; i++) {
		char	akey[16];

		sprintf(akey, "%d", i);
		insert_single("dkey_1_0", akey, 0, "data", strlen("data") + 1,
			      DAOS_TX_NONE, &req);
	}

	arg->rebuild_cb = reintegrate_inflight_io;
	arg->rebuild_cb_arg = &oid;
	drain_single_pool_target(arg, ranks_to_kill[0], tgt, false);
	for (i = 0; i < KEY_NR; i++) {
		char akey[32] = {0};
		char buf[16];

		sprintf(akey, "%d", i);
		/** Lookup */
		memset(buf, 0, 10);
		lookup_single("dkey_1_0", akey, 0, buf, 10, DAOS_TX_NONE, &req);
		assert_int_equal(req.iod[0].iod_size, strlen("data") + 1);

		/** Verify data consistency */
		assert_string_equal(buf, "data");
	}
	reintegrate_inflight_io_verify(arg);

	ioreq_fini(&req);
}

static void
drain_indexes(void **state)
{
	test_arg_t		*arg = *state;
	daos_obj_id_t		oid;
	struct ioreq		req;
	int			tgt = DEFAULT_FAIL_TGT;
	int			i;
	int			j;

	if (!test_runable(arg, 4))
		return;

	oid = daos_test_oid_gen(arg->coh, DAOS_OC_R1S_SPEC_RANK, 0, 0,
				arg->myrank);
	oid = dts_oid_set_rank(oid, ranks_to_kill[0]);
	oid = dts_oid_set_tgt(oid, tgt);
	ioreq_init(&req, arg->coh, oid, DAOS_IOD_ARRAY, arg);

	/** Insert 2000 records */
	print_message("Insert %d kv record in object "DF_OID"\n",
		      2000, DP_OID(oid));
	for (i = 0; i < KEY_NR; i++) {
		char	key[32] = {0};

		sprintf(key, "dkey_2_%d", i);
		for (j = 0; j < 20; j++)
			insert_single(key, "a_key", j, "data",
				      strlen("data") + 1, DAOS_TX_NONE, &req);
	}

	/* Drain rank 1 */
	arg->rebuild_cb = reintegrate_inflight_io;
	arg->rebuild_cb_arg = &oid;
	drain_single_pool_target(arg, ranks_to_kill[0], tgt, false);
	for (i = 0; i < KEY_NR; i++) {
		char	key[32] = {0};
		char	buf[16];

		sprintf(key, "dkey_2_%d", i);
		for (j = 0; j < 20; j++) {
			memset(buf, 0, 10);
			lookup_single(key, "a_key", j, buf, 10, DAOS_TX_NONE,
				      &req);
			assert_int_equal(req.iod[0].iod_size,
					 strlen("data") + 1);
			assert_string_equal(buf, "data");
		}
	}

	reintegrate_inflight_io_verify(arg);
	ioreq_fini(&req);
}

static void
drain_snap_update_keys(void **state)
{
	test_arg_t	*arg = *state;
	daos_obj_id_t	oid;
	struct ioreq	req;
	int		tgt = DEFAULT_FAIL_TGT;
	daos_epoch_t	snap_epoch[5];
	int		i;
	uint32_t	number;
	daos_key_desc_t kds[10];
	daos_anchor_t	anchor = { 0 };
	char		buf[256];
	int		buf_len = 256;


	if (!test_runable(arg, 4))
		return;

	oid = daos_test_oid_gen(arg->coh, DAOS_OC_R1S_SPEC_RANK, 0, 0,
				arg->myrank);
	oid = dts_oid_set_rank(oid, ranks_to_kill[0]);
	oid = dts_oid_set_tgt(oid, tgt);
	ioreq_init(&req, arg->coh, oid, DAOS_IOD_ARRAY, arg);
	/* Insert dkey/akey by different snapshot */
	for (i = 0; i < 5; i++) {
		char dkey[20] = { 0 };
		char akey[20] = { 0 };

		/* Update string for each snapshot */
		daos_cont_create_snap(arg->coh, &snap_epoch[i], NULL, NULL);
		sprintf(dkey, "dkey_%d", i);
		sprintf(akey, "akey_%d", i);
		insert_single(dkey, "a_key", 0, "data", 1, DAOS_TX_NONE, &req);
		insert_single("dkey", akey, 0, "data", 1, DAOS_TX_NONE, &req);
	}

	arg->rebuild_cb = reintegrate_inflight_io;
	arg->rebuild_cb_arg = &oid;
	drain_single_pool_target(arg, ranks_to_kill[0], tgt, false);

	for (i = 0; i < 5; i++) {
		daos_handle_t	th_open;

		memset(&anchor, 0, sizeof(anchor));
		daos_tx_open_snap(arg->coh, snap_epoch[i], &th_open, NULL);
		number = 10;
		enumerate_dkey(th_open, &number, kds, &anchor, buf,
			       buf_len, &req);

		assert_int_equal(number, i > 0 ? i + 1 : 0);

		number = 10;
		memset(&anchor, 0, sizeof(anchor));
		enumerate_akey(th_open, "dkey", &number, kds, &anchor,
			       buf, buf_len, &req);

		assert_int_equal(number, i);
		daos_tx_close(th_open, NULL);
	}
	number = 10;
	memset(&anchor, 0, sizeof(anchor));
	enumerate_dkey(DAOS_TX_NONE, &number, kds, &anchor, buf, buf_len, &req);
	assert_int_equal(number, 10);

	number = 10;
	memset(&anchor, 0, sizeof(anchor));
	enumerate_akey(DAOS_TX_NONE, "dkey", &number, kds, &anchor,
		       buf, buf_len, &req);
	assert_int_equal(number, 5);

	reintegrate_inflight_io_verify(arg);

	ioreq_fini(&req);
}

static void
drain_snap_punch_keys(void **state)
{
	test_arg_t	*arg = *state;
	daos_obj_id_t	oid;
	struct ioreq	req;
	int		tgt = DEFAULT_FAIL_TGT;
	daos_epoch_t	snap_epoch[5];
	int		i;
	daos_key_desc_t  kds[10];
	daos_anchor_t	 anchor;
	char		 buf[256];
	int		 buf_len = 256;
	uint32_t	 number;

	if (!test_runable(arg, 4))
		return;

	oid = daos_test_oid_gen(arg->coh, DAOS_OC_R3S_SPEC_RANK, 0, 0,
				arg->myrank);
	oid = dts_oid_set_rank(oid, ranks_to_kill[0]);
	oid = dts_oid_set_tgt(oid, tgt);
	ioreq_init(&req, arg->coh, oid, DAOS_IOD_ARRAY, arg);
	/* Insert dkey/akey */
	for (i = 0; i < 5; i++) {
		char dkey[20] = { 0 };
		char akey[20] = { 0 };
		char akey2[20] = { 0 };

		/* Update string for each snapshot */
		sprintf(dkey, "dkey_%d", i);
		sprintf(akey, "akey_%d", i);
		sprintf(akey2, "akey_%d", 100 + i);
		insert_single(dkey, "a_key", 0, "data", 1, DAOS_TX_NONE, &req);
		insert_single("dkey", akey, 0, "data", 1, DAOS_TX_NONE, &req);
		insert_single("dkey", akey2, 0, "data", 1, DAOS_TX_NONE, &req);
	}

	/* Insert dkey/akey by different epoch */
	for (i = 0; i < 5; i++) {
		char dkey[20] = { 0 };
		char akey[20] = { 0 };

		daos_cont_create_snap(arg->coh, &snap_epoch[i], NULL, NULL);

		sprintf(dkey, "dkey_%d", i);
		sprintf(akey, "akey_%d", i);
		punch_dkey(dkey, DAOS_TX_NONE, &req);
		punch_akey("dkey", akey, DAOS_TX_NONE, &req);
	}

	arg->rebuild_cb = reintegrate_inflight_io;
	arg->rebuild_cb_arg = &oid;
	drain_single_pool_target(arg, ranks_to_kill[0], tgt, false);

	for (i = 0; i < 5; i++) {
		daos_handle_t th_open;

		daos_tx_open_snap(arg->coh, snap_epoch[i], &th_open, NULL);
		number = 10;
		memset(&anchor, 0, sizeof(anchor));
		enumerate_dkey(th_open, &number, kds, &anchor, buf,
			       buf_len, &req);
		assert_int_equal(number, 6 - i);

		number = 10;
		memset(&anchor, 0, sizeof(anchor));
		enumerate_akey(th_open, "dkey", &number, kds,
			       &anchor, buf, buf_len, &req);
		assert_int_equal(number, 10 - i);

		daos_tx_close(th_open, NULL);
	}

	number = 10;
	memset(&anchor, 0, sizeof(anchor));
	enumerate_dkey(DAOS_TX_NONE, &number, kds, &anchor, buf,
		       buf_len, &req);
	assert_int_equal(number, 10);

	number = 10;
	memset(&anchor, 0, sizeof(anchor));
	enumerate_akey(DAOS_TX_NONE, "dkey", &number, kds, &anchor,
		       buf, buf_len, &req);
	assert_int_equal(number, 5);
	reintegrate_inflight_io_verify(arg);

	ioreq_fini(&req);
}

static void
drain_multiple(void **state)
{
	test_arg_t	*arg = *state;
	daos_obj_id_t	oid;
	struct ioreq	req;
	int		tgt = DEFAULT_FAIL_TGT;
	int		i;
	int		j;
	int		k;

	if (!test_runable(arg, 4))
		return;

	oid = daos_test_oid_gen(arg->coh, DAOS_OC_R1S_SPEC_RANK, 0, 0,
				arg->myrank);
	oid = dts_oid_set_rank(oid, ranks_to_kill[0]);
	oid = dts_oid_set_tgt(oid, tgt);
	ioreq_init(&req, arg->coh, oid, DAOS_IOD_ARRAY, arg);

	/** Insert 1000 records */
	print_message("Insert %d kv record in object "DF_OID"\n",
		      1000, DP_OID(oid));
	for (i = 0; i < 10; i++) {
		char	dkey[32] = {0};

		sprintf(dkey, "dkey_3_%d", i);
		for (j = 0; j < 10; j++) {
			char	akey[32] = {0};

			sprintf(akey, "akey_%d", j);
			for (k = 0; k < 10; k++)
				insert_single(dkey, akey, k, "data",
					      strlen("data") + 1,
					      DAOS_TX_NONE, &req);
		}
	}

	arg->rebuild_cb = reintegrate_inflight_io;
	arg->rebuild_cb_arg = &oid;
	drain_single_pool_target(arg, ranks_to_kill[0], tgt, false);
	for (i = 0; i < 10; i++) {
		char	dkey[32] = {0};

		sprintf(dkey, "dkey_3_%d", i);
		for (j = 0; j < 10; j++) {
			char	akey[32] = {0};
			char	buf[10];

			memset(buf, 0, 10);
			sprintf(akey, "akey_%d", j);
			for (k = 0; k < 10; k++) {
				lookup_single(dkey, akey, k, buf,
					      strlen("data") + 1,
					      DAOS_TX_NONE, &req);
				assert_int_equal(req.iod[0].iod_size,
						 strlen("data") + 1);
				assert_string_equal(buf, "data");
			}
		}
	}
	reintegrate_inflight_io_verify(arg);

	ioreq_fini(&req);
}

static void
drain_large_rec(void **state)
{
	test_arg_t		*arg = *state;
	daos_obj_id_t		oid;
	struct ioreq		req;
	int			tgt = DEFAULT_FAIL_TGT;
	int			i;
	char			buffer[5000];
	char			v_buffer[5000];

	if (!test_runable(arg, 4))
		return;

	oid = daos_test_oid_gen(arg->coh, DAOS_OC_R1S_SPEC_RANK, 0, 0,
				arg->myrank);
	oid = dts_oid_set_rank(oid, ranks_to_kill[0]);
	oid = dts_oid_set_tgt(oid, tgt);
	ioreq_init(&req, arg->coh, oid, DAOS_IOD_ARRAY, arg);

	/** Insert 1000 records */
	print_message("Insert %d kv record in object "DF_OID"\n",
		      KEY_NR, DP_OID(oid));
	memset(buffer, 'a', 5000);
	for (i = 0; i < KEY_NR; i++) {
		char	key[32] = {0};

		sprintf(key, "dkey_4_%d", i);
		insert_single(key, "a_key", 0, buffer, 5000, DAOS_TX_NONE,
			      &req);
	}

	arg->rebuild_cb = reintegrate_inflight_io;
	arg->rebuild_cb_arg = &oid;
	drain_single_pool_target(arg, ranks_to_kill[0], tgt, false);
	memset(v_buffer, 'a', 5000);
	for (i = 0; i < KEY_NR; i++) {
		char	key[32] = {0};

		sprintf(key, "dkey_4_%d", i);
		memset(buffer, 0, 5000);
		lookup_single(key, "a_key", 0, buffer, 5000, DAOS_TX_NONE,
			      &req);
		assert_memory_equal(v_buffer, buffer, 5000);
	}

	reintegrate_inflight_io_verify(arg);

	ioreq_fini(&req);
}

static void
drain_objects(void **state)
{
	test_arg_t	*arg = *state;
	daos_obj_id_t	oids[OBJ_NR];
	int		tgt = DEFAULT_FAIL_TGT;
	int		i;

	if (!test_runable(arg, 4))
		return;

	for (i = 0; i < OBJ_NR; i++) {
		oids[i] = daos_test_oid_gen(arg->coh, DAOS_OC_R1S_SPEC_RANK, 0,
					    0, arg->myrank);
		oids[i] = dts_oid_set_rank(oids[i], ranks_to_kill[0]);
		oids[i] = dts_oid_set_tgt(oids[i], DEFAULT_FAIL_TGT);
	}

	rebuild_io(arg, oids, OBJ_NR);
	arg->rebuild_cb = reintegrate_inflight_io;
	arg->rebuild_cb_arg = &oids[0];
	drain_single_pool_target(arg, ranks_to_kill[0], tgt, false);

	rebuild_io_validate(arg, oids, OBJ_NR);
	reintegrate_inflight_io_verify(arg);
}

static void
drain_fail_and_retry_objects(void **state)
{
	test_arg_t	*arg = *state;
	daos_obj_id_t	oids[OBJ_NR];
	int		i;

	if (!test_runable(arg, 4))
		return;

	for (i = 0; i < OBJ_NR; i++) {
		oids[i] = daos_test_oid_gen(arg->coh, DAOS_OC_R1S_SPEC_RANK, 0,
					    0, arg->myrank);
		oids[i] = dts_oid_set_rank(oids[i], ranks_to_kill[0]);
		oids[i] = dts_oid_set_tgt(oids[i], DEFAULT_FAIL_TGT);
	}

	rebuild_io(arg, oids, OBJ_NR);
	daos_debug_set_params(arg->group, -1, DMG_KEY_FAIL_LOC,
			      DAOS_REBUILD_OBJ_FAIL | DAOS_FAIL_ALWAYS, 0, NULL);

	drain_single_pool_rank(arg, ranks_to_kill[0], false);

	daos_debug_set_params(arg->group, -1, DMG_KEY_FAIL_LOC, 0, 0, NULL);
	rebuild_io_validate(arg, oids, OBJ_NR);

	drain_single_pool_rank(arg, ranks_to_kill[0], false);
	rebuild_io_validate(arg, oids, OBJ_NR);
}

/** create a new pool/container for each test */
static const struct CMUnitTest drain_tests[] = {
	{"DRAIN1: drain small rec multiple dkeys",
	 drain_dkeys, rebuild_small_sub_setup, test_teardown},
	{"DRAIN2: drain small rec multiple akeys",
	 drain_akeys, rebuild_small_sub_setup, test_teardown},
	{"DRAIN3: drain small rec multiple indexes",
	 drain_indexes, rebuild_small_sub_setup, test_teardown},
	{"DRAIN4: drain small rec multiple keys/indexes",
	 drain_multiple, rebuild_small_sub_setup, test_teardown},
	{"DRAIN5: drain large rec single index",
	 drain_large_rec, rebuild_small_sub_setup, test_teardown},
	{"DRAIN6: drain keys with multiple snapshots",
	 drain_snap_update_keys, rebuild_small_sub_setup, test_teardown},
	{"DRAIN7: drain keys/punch with multiple snapshots",
	 drain_snap_punch_keys, rebuild_small_sub_setup, test_teardown},
	{"DRAIN8: drain multiple objects",
	 drain_objects, rebuild_sub_setup, test_teardown},
	{"DRAIN9: drain fail and retry",
	 drain_fail_and_retry_objects, rebuild_sub_setup, test_teardown},
};

int
run_daos_drain_simple_test(int rank, int size, int *sub_tests,
			     int sub_tests_size)
{
	int rc = 0;

	par_barrier(PAR_COMM_WORLD);
	if (sub_tests_size == 0) {
		sub_tests_size = ARRAY_SIZE(drain_tests);
		sub_tests = NULL;
	}

	run_daos_sub_tests_only("DAOS_Drain_Simple", drain_tests,
				ARRAY_SIZE(drain_tests), sub_tests,
				sub_tests_size);

	par_barrier(PAR_COMM_WORLD);

	return rc;
}
