SCRIPT=$1
LOG_FILE=$2

TICK_PROCESSOR="/Users/shuai/github/v8-cmake/v8/tools/mac-tick-processor"

d8 ${SCRIPT} --prof \
--log-source-code \
--log-code \
--redirect-code-traces \
--print-opt-source \
--logfile ${LOG_FILE} \
--prepare-always-opt \
--max-inlined-bytecode-size=999999 \
--max-inlined-bytecode-size-cumulative=999999 \
--always-opt

${TICK_PROCESSOR} --preprocess ${LOG_FILE} > ${LOG_FILE}.json
