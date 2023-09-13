compile:
	@bash -c ' \
		source /opt/nfs/config/source_upcxx_2023.3.sh && \
		XX_GASNET_CONDUIT=udp upcxx -O2 -std=c++17 src/main.cpp -o build/main && \
		read -p "Do you want to create/update a nodes file? [N/y]: " createNodesFile; \
		createNodesFile=$${createNodesFile:-"N"}; \
		if [ "$$createNodesFile" == "y" ]; then \
			/opt/nfs/config/station204_name_list.sh 1 16 > nodes; \
		fi'

run:
	@bash -c 'source /opt/nfs/config/source_upcxx_2023.3.sh && \
		upcxx-run -shared-heap 256M -n 6 $$(upcxx-nodes nodes) build/main'
		

clean:
	@rm -f build/main
	@rm -f result.txt
