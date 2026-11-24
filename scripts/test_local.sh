SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
QC_STDLIB=$SCRIPT_DIR/../std_outs/std QC_STDLIB_LIBRARY=$SCRIPT_DIR/../std_outs/std/libstdqc.so $SCRIPT_DIR/../qc $1
