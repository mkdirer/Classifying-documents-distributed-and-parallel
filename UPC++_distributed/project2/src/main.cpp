#include <upcxx/upcxx.hpp>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>

std::vector<std::string> read_dictionary() {
	std::ifstream file("data/checkedWords.txt");
	if (!file.is_open()) {
        std::cerr << "Error opening file." << std::endl;

		for (int i = 0; i < upcxx::rank_n(); i++) {
			auto _ = upcxx::rpc(i, []() {upcxx::finalize();});
		}
        exit(1);
    }

	std::vector<std::string> words;
	std::string word;

	while (file >> word) {
        words.push_back(word);
    }

    file.close();

	return words;
}

std::vector<std::string> get_file_list() {
    std::vector<std::string> file_list;
    for (const auto& file : std::filesystem::directory_iterator("data")) {
        const auto name = std::string(file.path().filename());
        const auto ext = std::string(file.path().extension());
        if (name != "checkedWords.txt" && (ext == ".txt" || ext == ".css" || ext == ".html" || ext == ".odt" || ext == ".xml")) {
            file_list.push_back(file.path());
        }
    }
	return file_list;
}

std::vector<std::vector<upcxx::global_ptr<int>>> create_result_vec(int file_size, int dict_size) {
	std::vector<std::vector<upcxx::global_ptr<int>>> vec(file_size, std::vector<upcxx::global_ptr<int>>());
	for (auto& file : vec) {
		for (int i = 0; i < dict_size; i++) {
			file.push_back(upcxx::new_<int>(0));
		}
	}
	return vec;
}

bool isDelimiter(char c) {
    return !std::isalpha(c);
}

void process_file(std::vector<upcxx::global_ptr<int>> vec, std::string file_name, std::vector<std::string> dict) {
	std::ifstream file(file_name);
	if (!file.is_open()) {
		std::cerr << "Error opening file " << file_name << std::endl;

		for (int i = 0; i < upcxx::rank_n(); i++) {
			auto _ = upcxx::rpc(i, []() {upcxx::finalize();});
		}
	}

	std::vector<int> res(vec.size(), 0); // result array for each word from dictionary
	std::string word;
	char c;

	// read file and count dictonary words
	while (file.get(c)) {
		if (isDelimiter(c)) {
			if (!word.empty()) {
				auto it = std::find(dict.begin(), dict.end(), word);
				if (it != dict.end()) {
					int index = std::distance(dict.begin(), it);
					res[index]++;
				}
				word.clear();
			}
		} else {
			word += std::tolower(c);;
		}
	}
	file.close();

	int i = 0;
	for (auto& word : vec) {// word in dictionary, not in file
		res.push_back(1);
		auto _ = upcxx::rput(&res[i], word, 1); // ignore future
		i++;
	}
}

void manager() {
	// worker 0 - read dictionary
	upcxx::future<std::vector<std::string>> future_dict = upcxx::rpc(1, [](){return read_dictionary();});

	// get list of files to process
    std::vector<std::string> file_list = get_file_list();
	
	// get the dictionary
	std::vector<std::string> dictionary = future_dict.wait();

	// vector of pointers for each file for each word in dictionary
	std::vector<std::vector<upcxx::global_ptr<int>>> vec = create_result_vec(file_list.size(), dictionary.size());

	// assign each worker a file to process
	int worker_id = 1; // worker process number, starting from 1
	int i = 0;
	upcxx::future<> future = upcxx::make_future();
	for (auto& file : vec) {
		future = upcxx::when_all(future, upcxx::rpc(worker_id, [](std::vector<upcxx::global_ptr<int>> vec, std::string file_name, std::vector<std::string> dict){
			process_file(vec, file_name, dict);
		}, file, file_list[i], dictionary));

		if(worker_id + 1 == upcxx::rank_n())
			worker_id = 1;
		else
			worker_id ++;
		i++;
	}

	// wait for all values
	future.wait();

	// write results to a file and standard output
	std::ofstream outputFile("result.txt");
	i = 0;
	for (auto& file : vec) {
		std::cout << file_list[i] << std::endl;
		outputFile << file_list[i] << std::endl;
		for (auto& word : file) {
			std::cout << word.local()[0] << std::endl;
			outputFile << word.local()[0] << std::endl;
		}
		i++;
	}
	outputFile.close();
}

int main(int argc, char* argv[]) {
    upcxx::init();

    int num_workers = upcxx::rank_n();
    int rank_me = upcxx::rank_me();

    if (num_workers == 1) {
        std::cerr << "This program must be run with at least 2 processes" << std::endl;
        upcxx::finalize();
        return 1;
    }

    if (rank_me == 0) {
		manager();
    }

    upcxx::finalize();
    return 0;
}
