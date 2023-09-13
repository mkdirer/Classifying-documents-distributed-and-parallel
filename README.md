# Classifying-documents-distributed-and-parallel
Classifying documents in the form of a distributed application (UPC++) and a parallel application (MPI) of the Manager Workers type

## Example how to run:
ssh stud204-04 source /opt/nfs/config/source_upcxx_2023.3.sh XX_GASNET_CONDUIT=udp upcxx -O2 src/main.cpp -o build/main /opt/nfs/config/station204_name_list.sh 1 16 > nodes upcxx-run -shared-heap 256M -n 7 $(upcxx-nodes nodes) build/main
