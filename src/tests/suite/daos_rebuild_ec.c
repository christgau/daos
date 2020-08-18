/**
 * (C) Copyright 2016-2019 Intel Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * GOVERNMENT LICENSE RIGHTS-OPEN SOURCE SOFTWARE
 * The Government's rights to use, modify, reproduce, release, perform, display,
 * or disclose this software are subject to the terms of the Apache License as
 * provided in Contract No. B609815.
 * Any reproduction of computer software, computer software documentation, or
 * portions thereof marked with this legend must also reproduce the markings.
 */
/**
 * This file is for simple tests of rebuild, which does not need to kill the
 * rank, and only used to verify the consistency after different data model
 * rebuild.
 *
 * tests/suite/daos_rebuild_simple.c
 *
 *
 */
#define D_LOGFAC	DD_FAC(tests)

#include "daos_iotest.h"
#include <daos/pool.h>
#include <daos/mgmt.h>
#include <daos/container.h>

#define KEY_NR	10
static void
rebuild_partial_internal(void **state, bool parity)
{
	test_arg_t		*arg = *state;
	daos_obj_id_t		oid;
	struct ioreq		req;
	d_rank_t		kill_rank = 0;
	int			i;

	if (!test_runable(arg, 4))
		return;

	oid = dts_oid_gen(OC_EC_2P1G1, 0, arg->myrank);
	ioreq_init(&req, arg->coh, oid, DAOS_IOD_ARRAY, arg);

	print_message("Insert %d kv record in object "DF_OID"\n",
		      KEY_NR, DP_OID(oid));
	for (i = 0; i < KEY_NR; i++) {
		char	key[16];

		sprintf(key, "dkey_0_%d", i);
		insert_single(key, "a_key", i * 1048576, "data",
			      strlen("data") + 1, DAOS_TX_NONE, &req);
	}

	ioreq_fini(&req);
	kill_rank = get_killing_rank_by_oid(arg, oid, parity);
	rebuild_single_pool_target(arg, kill_rank, -1, false);
	ioreq_init(&req, arg->coh, oid, DAOS_IOD_ARRAY, arg);
	for (i = 0; i < KEY_NR; i++) {
		char	key[16];
		char	buf[10];

		sprintf(key, "dkey_0_%d", i);
		lookup_single(key, "a_key", i * 1048576, buf, 10,
			      DAOS_TX_NONE, &req);
		assert_int_equal(req.iod[0].iod_size, strlen("data") + 1);

		/** Verify data consistency */
		assert_string_equal(buf, "data");
	}
	ioreq_fini(&req);

	reintegrate_single_pool_target(arg, kill_rank, -1);
}

static void
rebuild_partial_fail_data(void **state)
{
	rebuild_partial_internal(state, false);
}

static void
rebuild_partial_fail_parity(void **state)
{
	rebuild_partial_internal(state, true);
}

#define DATA_SIZE	(1048576 * 2 + 512)
static void
rebuild_full_stripe_internal(void **state, bool parity)
{
	test_arg_t		*arg = *state;
	daos_obj_id_t		oid;
	struct ioreq		req;
	d_rank_t		kill_rank;
	d_rank_t		kill_rank1;
	int			i;

	if (!test_runable(arg, 5))
		return;

	oid = dts_oid_gen(OC_EC_2P1G1, 0, arg->myrank);
	ioreq_init(&req, arg->coh, oid, DAOS_IOD_ARRAY, arg);

	print_message("Insert %d kv record in object "DF_OID"\n",
		      KEY_NR, DP_OID(oid));
	for (i = 0; i < 2; i++) {
		char		key[16];
		char		data[DATA_SIZE];
		daos_recx_t	recx;

		sprintf(key, "dkey_2_1M_%d", i);
		recx.rx_idx = 0;
		recx.rx_nr = DATA_SIZE;

		memset(data, 'a', DATA_SIZE);
		insert_recxs(key, "a_key_1M", 1, DAOS_TX_NONE, &recx, 1,
			     data, DATA_SIZE, &req);

	}

	ioreq_fini(&req);

	kill_rank = get_killing_rank_by_oid(arg, oid, parity);
	rebuild_single_pool_target(arg, kill_rank, -1, false);

	ioreq_init(&req, arg->coh, oid, DAOS_IOD_ARRAY, arg);
	if (parity) {
		kill_rank1 = get_killing_rank_by_oid(arg, oid, false);
		rebuild_single_pool_target(arg, kill_rank1, -1, false);
	}

	for (i = 0; i < 2; i++) {
		char	key[16];
		char	data[DATA_SIZE];
		char	verify_data[DATA_SIZE];

		sprintf(key, "dkey_2_1M_%d", i);
		memset(verify_data, 'a', DATA_SIZE);
		memset(data, 0, DATA_SIZE);
		lookup_single_with_rxnr(key, "a_key_1M", 0, data,
					1, DATA_SIZE, DAOS_TX_NONE, &req);
		assert_int_equal(req.iod[0].iod_size, 1);

		/** Verify data consistency */
		assert_memory_equal(data, verify_data, DATA_SIZE);
	}

	ioreq_fini(&req);
	reintegrate_single_pool_target(arg, kill_rank, -1);
	if (parity)
		reintegrate_single_pool_target(arg, kill_rank1, -1);
}

static void
rebuild_full_stripe_fail_data(void **state)
{
	rebuild_full_stripe_internal(state, false);
}

static void
rebuild_full_stripe_fail_parity(void **state)
{
	rebuild_full_stripe_internal(state, true);
}

/** create a new pool/container for each test */
static const struct CMUnitTest rebuild_tests[] = {
	{"REBUILD0: rebuild partial update with data tgt fail",
	 rebuild_partial_fail_data, rebuild_small_sub_setup, test_teardown},
	{"REBUILD1: rebuild partial update with parity tgt fail",
	 rebuild_partial_fail_parity, rebuild_small_sub_setup, test_teardown},
	{"REBUILD2: rebuild full stripe update with data tgt fail",
	 rebuild_full_stripe_fail_data, rebuild_small_sub_setup, test_teardown},
	{"REBUILD3: rebuild full stripe update with data tgt fail",
	 rebuild_full_stripe_fail_parity, rebuild_small_sub_setup,
	 test_teardown},
};

int
run_daos_rebuild_simple_ec_test(int rank, int size, int *sub_tests,
				int sub_tests_size)
{
	int rc = 0;

	MPI_Barrier(MPI_COMM_WORLD);
	if (sub_tests_size == 0) {
		sub_tests_size = ARRAY_SIZE(rebuild_tests);
		sub_tests = NULL;
	}

	run_daos_sub_tests_only("DAOS rebuild ec tests", rebuild_tests,
				ARRAY_SIZE(rebuild_tests), sub_tests,
				sub_tests_size);

	MPI_Barrier(MPI_COMM_WORLD);

	return rc;
}
