#SCRIPT=service.base2.js
SCRIPT=$1

echo "node:"
time node ${SCRIPT}

echo "release:"
time ../cmake-build-release/d8 ${SCRIPT}

echo "debug:"
time ../cmake-build-debug/d8 ${SCRIPT}
