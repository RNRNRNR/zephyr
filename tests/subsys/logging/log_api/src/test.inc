/*
 * Copyright (c) 2021 Nordic Semiconductor
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mock_backend.h"
#include "test_module.h"
#include <ztest.h>
#include <logging/log.h>
#include <logging/log_ctrl.h>

#ifndef CONFIG_LOG_BUFFER_SIZE
#define CONFIG_LOG_BUFFER_SIZE 4
#endif

#define MODULE_NAME test
#define CXX_MODULE_NAME test_cxx

#if __cplusplus
LOG_MODULE_REGISTER(CXX_MODULE_NAME, CONFIG_SAMPLE_MODULE_LOG_LEVEL);
#else
LOG_MODULE_REGISTER(MODULE_NAME, CONFIG_SAMPLE_MODULE_LOG_LEVEL);
#endif

#define LOG2_SIMPLE_MSG_LEN \
	ROUND_UP(sizeof(struct log_msg2_hdr) + 2 * sizeof(void *), sizeof(long long))

#ifdef CONFIG_LOG_TIMESTAMP_64BIT
#define TIMESTAMP_INIT_VAL 0x100000000
#else
#define TIMESTAMP_INIT_VAL 0
#endif

MOCK_LOG_BACKEND_DEFINE(backend1, false);
MOCK_LOG_BACKEND_DEFINE(backend2, false);

static log_timestamp_t stamp;
static bool in_panic;
static int16_t test_source_id;
static int16_t test_cxx_source_id;

static log_timestamp_t timestamp_get(void)
{
	return stamp++;
}

/**
 * @brief Function for finding source ID based on source name.
 *
 * @param name Source name
 *
 * @return Source ID.
 */
static int16_t log_source_id_get(const char *name)
{
	int16_t count = (int16_t)log_src_cnt_get(CONFIG_LOG_DOMAIN_ID);

	for (int16_t i = 0; i < count; i++) {
		if (strcmp(log_source_name_get(CONFIG_LOG_DOMAIN_ID, i), name)
		    == 0) {
			return i;
		}
	}
	return -1;
}

/**
 * @brief Flush logs.
 *
 * If processing thread is enabled keep sleeping until there are no pending messages
 * else process logs here.
 */
static void flush_log(void)
{
	if (IS_ENABLED(CONFIG_LOG_PROCESS_THREAD)) {
		while (log_data_pending()) {
			k_msleep(100);
		}
		k_msleep(100);
	} else {
		while (LOG_PROCESS()) {
		}
	}
}

static void log_setup(bool backend2_enable)
{
	stamp = TIMESTAMP_INIT_VAL;
	zassert_false(in_panic, "Logger in panic state.");

	log_core_init();
	log_init();

	zassert_equal(0, log_set_timestamp_func(timestamp_get, 0),
		      "Expects successful timestamp function setting.");

	mock_log_backend_reset(&backend1);
	mock_log_backend_reset(&backend2);

	log_backend_enable(&backend1, backend1.cb->ctx, LOG_LEVEL_DBG);

	if (backend2_enable) {
		log_backend_enable(&backend2, backend2.cb->ctx, LOG_LEVEL_DBG);
	} else {
		log_backend_disable(&backend2);
	}

	test_source_id = log_source_id_get(STRINGIFY(MODULE_NAME));
	test_cxx_source_id = log_source_id_get(STRINGIFY(CXX_MODULE_NAME));
}

/**
 * @brief Process and validate that backends got exepcted content.
 *
 * @param backend2_enable If true backend2 is also validated.
 * @param panic True means that logging is in panic mode, flushing is skipped.
 */
static void process_and_validate(bool backend2_enable, bool panic)
{
	if (!panic) {
		flush_log();
	}

	mock_log_backend_validate(&backend1, panic);

	if (backend2_enable) {
		mock_log_backend_validate(&backend2, panic);
	}
}

static void test_log_various_messages(void)
{
	char str[128];
	char dstr[] = "abcd";
	int8_t i = -5;
	log_timestamp_t exp_timestamp = TIMESTAMP_INIT_VAL;

	log_setup(false);

#ifdef CONFIG_LOG2
	unsigned long long ull = 0x1122334455667799;
	long long ll = -12313213214454545;

#define TEST_MSG_0 "%lld %llu %hhd"
#define TEST_MSG_0_PREFIX "%s: %lld %llu %hhd"
#define TEST_MSG_1 "%f %d %f"

	if (IS_ENABLED(CONFIG_SAMPLE_MODULE_LOG_LEVEL_DBG)) {
		/* If prefix is enabled, add function name prefix */
		if (IS_ENABLED(CONFIG_LOG_FUNC_NAME_PREFIX_DBG)) {
			snprintk(str, sizeof(str),
				 TEST_MSG_0_PREFIX, __func__, ll, ull, i);
		} else {
			snprintk(str, sizeof(str), TEST_MSG_0, ll, ull, i);
		}

		mock_log_backend_record(&backend1, LOG_CURRENT_MODULE_ID(),
					CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_DBG,
					exp_timestamp++, str);
	}

	LOG_DBG(TEST_MSG_0, ll, ull, i);

#ifdef CONFIG_FPU
	float f = -1.2356;
	double d = -1.2356;

	snprintk(str, sizeof(str), TEST_MSG_1, f, 100,  d);
	mock_log_backend_record(&backend1, LOG_CURRENT_MODULE_ID(),
				CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_INF,
				exp_timestamp++, str);

	LOG_INF(TEST_MSG_1, f, 100, d);
#endif /* CONFIG_FPU */

#else /* CONFIG_LOG2 */

#define TEST_MSG_0 "%hhd"
#define TEST_MSG_0_PREFIX "%s: %hhd"
#define TEST_MSG_1 "%p"
	if (IS_ENABLED(CONFIG_SAMPLE_MODULE_LOG_LEVEL_DBG)) {
		/* If prefix is enabled, add function name prefix */
		if (IS_ENABLED(CONFIG_LOG_FUNC_NAME_PREFIX_DBG)) {
			snprintk(str, sizeof(str),
				 TEST_MSG_0_PREFIX, __func__, i);
		} else {
			snprintk(str, sizeof(str), TEST_MSG_0, i);
		}

		mock_log_backend_record(&backend1, LOG_CURRENT_MODULE_ID(),
					CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_DBG,
					exp_timestamp++, str);
	}

	LOG_DBG(TEST_MSG_0, i);

	snprintk(str, sizeof(str), TEST_MSG_1, &i);
	mock_log_backend_record(&backend1, LOG_CURRENT_MODULE_ID(),
				CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_INF,
				exp_timestamp++, str);
	LOG_INF(TEST_MSG_1, &i);
#endif
	snprintk(str, sizeof(str), "wrn %s", dstr);
	mock_log_backend_record(&backend1, LOG_CURRENT_MODULE_ID(),
				CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_WRN,
				exp_timestamp++, str);

	mock_log_backend_record(&backend1, LOG_CURRENT_MODULE_ID(),
				CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_ERR,
				exp_timestamp++, "err");

	LOG_WRN("wrn %s", log_strdup(dstr));
	dstr[0] = '\0';

	LOG_ERR("err");

	process_and_validate(false, false);
}

/*
 * Test is using 2 backends and runtime filtering is enabled. After first call
 * filtering for backend2 is reduced to warning. It is expected that next INFO
 * level log message will be passed only to backend1.
 */
static void test_log_backend_runtime_filtering(void)
{
	log_timestamp_t exp_timestamp = TIMESTAMP_INIT_VAL;

	if (!IS_ENABLED(CONFIG_LOG_RUNTIME_FILTERING)) {
		ztest_test_skip();
	}

	log_setup(true);

	if (IS_ENABLED(CONFIG_SAMPLE_MODULE_LOG_LEVEL_DBG)) {
		char str[128];

		/* If prefix is enabled, add function name prefix */
		if (IS_ENABLED(CONFIG_LOG_FUNC_NAME_PREFIX_DBG)) {
			snprintk(str, sizeof(str),
				 "%s: test", __func__);
		} else {
			snprintk(str, sizeof(str), "test");
		}

		mock_log_backend_record(&backend1, LOG_CURRENT_MODULE_ID(),
				CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_DBG,
				exp_timestamp, str);
		mock_log_backend_record(&backend2, LOG_CURRENT_MODULE_ID(),
				CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_DBG,
				exp_timestamp, str);
		exp_timestamp++;
	}

	mock_log_backend_record(&backend1, LOG_CURRENT_MODULE_ID(),
				CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_INF,
				exp_timestamp, "test");
	mock_log_backend_record(&backend2, LOG_CURRENT_MODULE_ID(),
				CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_INF,
				exp_timestamp, "test");
	exp_timestamp++;

	LOG_DBG("test");
	LOG_INF("test");

	process_and_validate(true, false);

	log_filter_set(&backend2,
			CONFIG_LOG_DOMAIN_ID,
			LOG_CURRENT_MODULE_ID(),
			LOG_LEVEL_WRN);

	uint8_t data[] = {1, 2, 4, 5, 6, 8};

	/* INF logs expected only on backend1 */
	mock_log_backend_record(&backend1, LOG_CURRENT_MODULE_ID(),
				CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_INF,
				exp_timestamp++, "test");
	mock_log_backend_generic_record(&backend1, LOG_CURRENT_MODULE_ID(),
					CONFIG_LOG_DOMAIN_ID,
					LOG_LEVEL_INF,
					exp_timestamp++, "hexdump",
					data, sizeof(data));

	mock_log_backend_record(&backend1, LOG_CURRENT_MODULE_ID(),
				CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_WRN,
				exp_timestamp, "test2");
	mock_log_backend_record(&backend2, LOG_CURRENT_MODULE_ID(),
				CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_WRN,
				exp_timestamp++, "test2");

	mock_log_backend_generic_record(&backend1, LOG_CURRENT_MODULE_ID(),
					CONFIG_LOG_DOMAIN_ID,
					LOG_LEVEL_WRN,
					exp_timestamp, "hexdump",
					data, sizeof(data));
	mock_log_backend_generic_record(&backend2, LOG_CURRENT_MODULE_ID(),
					CONFIG_LOG_DOMAIN_ID,
					LOG_LEVEL_WRN,
					exp_timestamp++, "hexdump",
					data, sizeof(data));

	LOG_INF("test");
	LOG_HEXDUMP_INF(data, sizeof(data), "hexdump");
	LOG_WRN("test2");
	LOG_HEXDUMP_WRN(data, sizeof(data), "hexdump");

	process_and_validate(true, false);
}

static size_t get_max_hexdump(void)
{
	if (IS_ENABLED(CONFIG_LOG2)) {
		return CONFIG_LOG_BUFFER_SIZE - sizeof(struct log_msg2_hdr);
	}

	uint32_t msgs_in_buf = CONFIG_LOG_BUFFER_SIZE/sizeof(union log_msg_chunk);

	return LOG_MSG_HEXDUMP_BYTES_HEAD_CHUNK +
			    HEXDUMP_BYTES_CONT_MSG * (msgs_in_buf - 1);
}

#if defined(__sparc__) || defined(CONFIG_ARCH_POSIX)
#define STR_SIZE(s) (strlen(s) + 2 * sizeof(char))
#else
#define STR_SIZE(s) 0
#endif

static size_t get_long_hexdump(void)
{
	if (IS_ENABLED(CONFIG_LOG2)) {
		return CONFIG_LOG_BUFFER_SIZE -
			/* First message */
			ROUND_UP(LOG2_SIMPLE_MSG_LEN + 2 * sizeof(int) + STR_SIZE("test %d"),
				 sizeof(long long)) -
			/* Hexdump message excluding data */
			ROUND_UP(LOG2_SIMPLE_MSG_LEN + STR_SIZE("hexdump"),
				 sizeof(long long)) - 2 * sizeof(int);
	}

	uint32_t msgs_in_buf = (uint32_t)CONFIG_LOG_BUFFER_SIZE / sizeof(union log_msg_chunk);

	return LOG_MSG_HEXDUMP_BYTES_HEAD_CHUNK +
		HEXDUMP_BYTES_CONT_MSG * (msgs_in_buf - 1) -
		HEXDUMP_BYTES_CONT_MSG;
}

/*
 * When LOG_MODE_OVERFLOW is enabled, logger should discard oldest messages when
 * there is no room. However, if after discarding all messages there is still no
 * room then current log is discarded.
 */
static uint8_t data[CONFIG_LOG_BUFFER_SIZE];

static void test_log_overflow(void)
{
	log_timestamp_t exp_timestamp = TIMESTAMP_INIT_VAL;

	if (IS_ENABLED(CONFIG_LOG_MODE_IMMEDIATE)) {
		ztest_test_skip();
	}

	if (!IS_ENABLED(CONFIG_LOG_MODE_OVERFLOW)) {
		ztest_test_skip();
	}

	for (int i = 0; i < CONFIG_LOG_BUFFER_SIZE; i++) {
		data[i] = i;
	}

	log_setup(false);

	uint32_t hexdump_len = get_long_hexdump();

	/* expect first message to be dropped */
	exp_timestamp++;
	mock_log_backend_generic_record(&backend1, LOG_CURRENT_MODULE_ID(),
					CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_INF,
					exp_timestamp++, "hexdump",
					data, hexdump_len);
	mock_log_backend_record(&backend1, LOG_CURRENT_MODULE_ID(),
				CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_INF,
				exp_timestamp++, "test2");
	mock_log_backend_drop_record(&backend1, 1);

	LOG_INF("test %d %d", 100, 100);
	LOG_HEXDUMP_INF(data, hexdump_len, "hexdump");
	LOG_INF("test2");

	process_and_validate(false, false);

	log_setup(false);

	exp_timestamp = TIMESTAMP_INIT_VAL;
	hexdump_len = get_max_hexdump();
	mock_log_backend_reset(&backend1);
	if (IS_ENABLED(CONFIG_LOG2_DEFERRED)) {
		/* Log2 allocation is not destructive if request exceeds the
		 * capacity.
		 */
		mock_log_backend_record(&backend1, LOG_CURRENT_MODULE_ID(),
					CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_INF,
					exp_timestamp, "test");
		mock_log_backend_drop_record(&backend1, 1);
	} else {
		/* Expect big message to be dropped because it does not fit in.
		 * First message is also dropped in the process of finding free space.
		 */
		mock_log_backend_drop_record(&backend1, 2);
	}

	LOG_INF("test");
	LOG_HEXDUMP_INF(data, hexdump_len+1, "test");

	process_and_validate(false, false);
}

/*
 * Test checks if arguments are correctly processed by the logger.
 *
 * Log messages with supported number of messages are called. Test backend
 * validates number of arguments and values.
 */
#define MOCK_LOG_BACKEND_RECORD(str) mock_log_backend_record( \
	&backend1, LOG_CURRENT_MODULE_ID(), \
	CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_INF, \
	exp_timestamp++, \
	str);

static void test_log_arguments(void)
{
	log_timestamp_t exp_timestamp = TIMESTAMP_INIT_VAL;

	log_setup(false);

	MOCK_LOG_BACKEND_RECORD("test");
	MOCK_LOG_BACKEND_RECORD("test 1 2 3");
	MOCK_LOG_BACKEND_RECORD("test 1 2 3 4");
	MOCK_LOG_BACKEND_RECORD("test 1 2 3 4 5");

	LOG_INF("test");
	LOG_INF("test %d %d %d", 1, 2, 3);
	LOG_INF("test %d %d %d %d", 1, 2, 3, 4);
	LOG_INF("test %d %d %d %d %d", 1, 2, 3, 4, 5);

	process_and_validate(false, false);

	MOCK_LOG_BACKEND_RECORD("test 1 2 3 4 5 6");
	MOCK_LOG_BACKEND_RECORD("test 1 2 3 4 5 6 7");
	MOCK_LOG_BACKEND_RECORD("test 1 2 3 4 5 6 7 8");
	MOCK_LOG_BACKEND_RECORD("test 1 2 3 4 5 6 7 8 9");

	LOG_INF("test %d %d %d %d %d %d", 1, 2, 3, 4, 5, 6);
	LOG_INF("test %d %d %d %d %d %d %d", 1, 2, 3, 4, 5, 6, 7);
	LOG_INF("test %d %d %d %d %d %d %d %d", 1, 2, 3, 4, 5, 6, 7,8);
	LOG_INF("test %d %d %d %d %d %d %d %d %d", 1, 2, 3, 4, 5, 6, 7,8, 9);

	process_and_validate(false, false);

	MOCK_LOG_BACKEND_RECORD("test 1 2 3 4 5 6 7 8 9 10");
	MOCK_LOG_BACKEND_RECORD("test 1 2 3 4 5 6 7 8 9 10 11");
	MOCK_LOG_BACKEND_RECORD("test 1 2 3 4 5 6 7 8 9 10 11 12");

	LOG_INF("test %d %d %d %d %d %d %d %d %d %d",
		1, 2, 3, 4, 5, 6, 7,8, 9, 10);
	LOG_INF("test %d %d %d %d %d %d %d %d %d %d %d",
		1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11);
	LOG_INF("test %d %d %d %d %d %d %d %d %d %d %d %d",
		1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);

	process_and_validate(false, false);

	MOCK_LOG_BACKEND_RECORD("test 1 2 3 4 5 6 7 8 9 10 11 12 13");
	MOCK_LOG_BACKEND_RECORD("test 1 2 3 4 5 6 7 8 9 10 11 12 13 14");
	LOG_INF("test %d %d %d %d %d %d %d %d %d %d %d %d %d",
		1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13);
	LOG_INF("test %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
		1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14);

	process_and_validate(false, false);
}

/* Function comes from the file which is part of test module. It is
 * expected that logs coming from it will have same source_id as current
 * module (this file).
 */
static void test_log_from_declared_module(void)
{
	log_timestamp_t exp_timestamp = TIMESTAMP_INIT_VAL;

	log_setup(false);

	/* See test module for log message content. */
	if (IS_ENABLED(CONFIG_SAMPLE_MODULE_LOG_LEVEL_DBG)) {
		char str[128];

		/* If prefix is enabled, add function name prefix */
		if (IS_ENABLED(CONFIG_LOG_FUNC_NAME_PREFIX_DBG)) {
			snprintk(str, sizeof(str),
				 "%s: " TEST_DBG_MSG, "test_func");
		} else {
			snprintk(str, sizeof(str), TEST_DBG_MSG);
		}

		mock_log_backend_record(&backend1, test_source_id,
					CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_DBG,
					exp_timestamp++, str);
	}

	mock_log_backend_record(&backend1, test_source_id,
				CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_ERR,
				exp_timestamp++, TEST_ERR_MSG);

	test_func();

	if (IS_ENABLED(CONFIG_SAMPLE_MODULE_LOG_LEVEL_DBG)) {
		char str[128];

		/* If prefix is enabled, add function name prefix */
		if (IS_ENABLED(CONFIG_LOG_FUNC_NAME_PREFIX_DBG)) {
			snprintk(str, sizeof(str),
				 "%s: " TEST_INLINE_DBG_MSG, "test_inline_func");
		} else {
			snprintk(str, sizeof(str), TEST_INLINE_DBG_MSG);
		}

		mock_log_backend_record(&backend1, test_source_id,
					CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_DBG,
					exp_timestamp++, str);
	}

	mock_log_backend_record(&backend1, test_source_id,
				CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_ERR,
				exp_timestamp, TEST_INLINE_ERR_MSG);

	test_inline_func();

	process_and_validate(false, false);
}

/** Calculate how many messages will fit in the buffer. Also calculate if
 * remaining free space is size of message or not. This impacts how many messages
 * are dropped. If free space is equal to message size then when buffer is full,
 * adding new message will lead to one message drop, otherwise 2 message will
 * be dropped.
 */
static size_t get_short_msg_capacity(bool *remainder)
{
	if (IS_ENABLED(CONFIG_LOG2)) {
		*remainder = (CONFIG_LOG_BUFFER_SIZE % LOG2_SIMPLE_MSG_LEN) ?
				true : false;

		return (CONFIG_LOG_BUFFER_SIZE - sizeof(int)) / LOG2_SIMPLE_MSG_LEN;
	}

	*remainder = (CONFIG_LOG_BUFFER_SIZE % sizeof(struct log_msg)) ?
			true : false;

	return CONFIG_LOG_BUFFER_SIZE / sizeof(struct log_msg);
}

static void log_n_messages(uint32_t n_msg, uint32_t exp_dropped)
{
	log_timestamp_t exp_timestamp = TIMESTAMP_INIT_VAL;

	log_setup(false);

	stamp = TIMESTAMP_INIT_VAL;

	for (uint32_t i = 0; i < n_msg; i++) {
		if (i >= exp_dropped) {
			mock_log_backend_record(&backend1, LOG_CURRENT_MODULE_ID(),
				CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_INF,
				exp_timestamp, "dummy");
		}
		exp_timestamp++;
		LOG_INF("dummy");
	}

	mock_log_backend_drop_record(&backend1, exp_dropped);

	process_and_validate(false, false);
	mock_log_backend_reset(&backend1);
}

/*
 * Test checks if backend receives notification about dropped messages. It
 * first blocks threads to ensure full control of log processing time and
 * then logs certain log messages, expecting dropped notification.
 */
static void test_log_msg_dropped_notification(void)
{
	if (IS_ENABLED(CONFIG_LOG_MODE_IMMEDIATE)) {
		ztest_test_skip();
	}

	if (!IS_ENABLED(CONFIG_LOG_MODE_OVERFLOW)) {
		ztest_test_skip();
	}

	bool remainder;
	uint32_t capacity = get_short_msg_capacity(&remainder);

	log_n_messages(capacity, 0);

	/* Expect messages dropped when logger more than buffer capacity. */
	log_n_messages(capacity + 1, 1 + (remainder ? 1 : 0));
	log_n_messages(capacity + 2, 2 + (remainder ? 1 : 0));
}

/* Test checks if panic is correctly executed. On panic logger should flush all
 * messages and process logs in place (not in deferred way).
 */
static void test_log_panic(void)
{
	log_timestamp_t exp_timestamp = TIMESTAMP_INIT_VAL;

	log_setup(false);

	mock_log_backend_record(&backend1, LOG_CURRENT_MODULE_ID(),
				CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_WRN,
				exp_timestamp++, "test");
	mock_log_backend_record(&backend1, LOG_CURRENT_MODULE_ID(),
				CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_WRN,
				exp_timestamp++, "test");
	LOG_WRN("test");
	LOG_WRN("test");

	/* logs should be flushed in panic */
	log_panic();

	process_and_validate(false, true);

	/* messages processed where called */
	mock_log_backend_record(&backend1, LOG_CURRENT_MODULE_ID(),
				CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_WRN,
				exp_timestamp++, "test");
	LOG_WRN("test");

	process_and_validate(false, true);
}

static void test_log_printk(void)
{
	if (!IS_ENABLED(CONFIG_LOG_PRINTK)) {
		ztest_test_skip();
	}

	log_timestamp_t exp_timestamp = TIMESTAMP_INIT_VAL;

	log_setup(false);

	mock_log_backend_record(&backend1, 0,
				CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_INTERNAL_RAW_STRING,
				exp_timestamp++, "test 100");
	printk("test %d", 100);

	log_panic();

	mock_log_backend_record(&backend1, 0,
				CONFIG_LOG_DOMAIN_ID, LOG_LEVEL_INTERNAL_RAW_STRING,
				exp_timestamp++, "test 101");
	printk("test %d", 101);

	process_and_validate(false, true);
}

/* Disable backends because same suite may be executed again but compiled by C++ */
static void log_api_suite_teardown(void *data)
{
	ARG_UNUSED(data);
	log_backend_disable(&backend1);
	log_backend_disable(&backend2);
}

static void *log_api_suite_setup(void)
{
	PRINT("Configuration:\n");
	PRINT("\t Mode: %s\n",
	      IS_ENABLED(CONFIG_LOG_MODE_IMMEDIATE) ? "Immediate" : "Deferred");
	PRINT("\t Version: %s\n",
	      IS_ENABLED(CONFIG_LOG2) ? "v2" : "v1");
	PRINT("\t Runtime filtering: %s\n",
	      IS_ENABLED(CONFIG_LOG_RUNTIME_FILTERING) ? "yes" : "no");
	PRINT("\t Overwrite: %s\n",
	      IS_ENABLED(CONFIG_LOG_MODE_OVERFLOW) ? "yes" : "no");
#if __cplusplus
	PRINT("\t C++: yes\n");
#endif
	flush_log();

	return NULL;
}

static void log_api_suite_before(void *data)
{
	ARG_UNUSED(data);

	/* Flush logs and enable test backends. */
	flush_log();
	mock_log_backend_check_enable(&backend1);
	mock_log_backend_check_enable(&backend2);
}

static void log_api_suite_after(void *data)
{

	ARG_UNUSED(data);
	/* Disable testing backends after the test. Otherwise test may fail due
	 * to unexpected log message.
	 */
	mock_log_backend_check_disable(&backend1);
	mock_log_backend_check_disable(&backend2);
}

#define WRAP_TEST(test_name, suffix)                       \
	ZTEST(UTIL_CAT(test_log_api_, suffix), UTIL_CAT(test_name, UTIL_CAT(_,suffix))) \
	{                                                  \
		test_name();                               \
	}

#if __cplusplus
#define TEST_SUFFIX cxx
#else
#define TEST_SUFFIX cc
#endif
#define TEST_SUITE_NAME UTIL_CAT(test_log_api_, TEST_SUFFIX)

ZTEST_SUITE(TEST_SUITE_NAME, NULL, log_api_suite_setup,
	    log_api_suite_before, log_api_suite_after, log_api_suite_teardown);
WRAP_TEST(test_log_various_messages, TEST_SUFFIX)
WRAP_TEST(test_log_backend_runtime_filtering, TEST_SUFFIX)
WRAP_TEST(test_log_overflow, TEST_SUFFIX)
WRAP_TEST(test_log_arguments, TEST_SUFFIX)
WRAP_TEST(test_log_from_declared_module, TEST_SUFFIX)
WRAP_TEST(test_log_msg_dropped_notification, TEST_SUFFIX)
WRAP_TEST(test_log_panic, TEST_SUFFIX)
WRAP_TEST(test_log_printk, TEST_SUFFIX)
