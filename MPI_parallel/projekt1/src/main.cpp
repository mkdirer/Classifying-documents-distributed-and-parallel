#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <mpi.h>

enum Tags {
    DICTIONARY_SIZE_MESSAGE = 0,
    SOURCE_FILE_MESSAGE = 1,
    FILE_VECTOR_MESSAGE = 2,
    EMPTY_MESSAGE = 3,
};

void manager(int argc, char* argv[], int num_nodes) {
    // Wait for a message from worker0 with the dictionary and read file_list in the background
    MPI_Status status;
    int dict_size;
    MPI_Request dict_size_request;

    MPI_Irecv(&dict_size, 1, MPI_INT, MPI_ANY_SOURCE, Tags::DICTIONARY_SIZE_MESSAGE, MPI_COMM_WORLD, &dict_size_request);
    // Manager => Reading files

    std::vector<std::string> file_list;

    std::cout << "Reading file list" << std::endl;

    for (const auto& file : std::filesystem::directory_iterator("data")) {
        const auto name = std::string(file.path().filename());
        const auto ext = std::string(file.path().extension());
        if (name != "checkedWords.txt" && (ext == ".txt" || ext == ".css" || ext == ".html" || ext == ".odt" || ext == ".xml")) {
            file_list.push_back(file.path());
        }
    }

    std::cout << "Read files. Waiting for information about the dictionary" << std::endl;

    // Wait for the dictionary
    MPI_Wait(&dict_size_request, &status);

    std::cout << "Have dictionary information, allocating required memory" << std::endl;

    auto result_container = new int[dict_size];

    std::vector<std::vector<int>> files_results;

    files_results.resize(file_list.size());

    for (auto& result : files_results) {
        result.resize(dict_size);
    }

    int completed_processes  = 0;
    int assigned_files_count = 0;

    int current_source;
    int current_tag;

    auto assigned_file_indices = new int[num_nodes];

    char* current_file_name;

    do {
        MPI_Recv(result_container, dict_size, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        current_source = status.MPI_SOURCE;
        current_tag = status.MPI_TAG;

        std::cout << "Received message from " << current_source << " Tag: " << current_tag << std::endl;

        if (current_tag == Tags::FILE_VECTOR_MESSAGE) {
            // Save result
            for (unsigned int i = 0; i < dict_size; i++) {
                files_results[assigned_file_indices[current_source]][i] = result_container[i];
            }
        }

        if (assigned_files_count < file_list.size()) {
            current_file_name = new char[file_list[assigned_files_count].size()];
            std::strcpy(current_file_name, file_list[assigned_files_count].c_str());
            // There is work to be done - assign it
            MPI_Send(current_file_name, static_cast<int>(file_list[assigned_files_count].size()), MPI_CHAR, current_source, Tags::SOURCE_FILE_MESSAGE, MPI_COMM_WORLD);

            std::cout << "Assigned file " << file_list[assigned_files_count] << " to " << current_source << std::endl;

            assigned_file_indices[current_source] = assigned_files_count;
            assigned_files_count++;
            delete [] current_file_name;
        } else {
            // No more work - terminate
            MPI_Send(nullptr, 0, MPI_CHAR, current_source, Tags::SOURCE_FILE_MESSAGE, MPI_COMM_WORLD);
            
            std::cout << "Telling node to shut up " << current_source << " disable/count " << completed_processes  << "/" << (num_nodes - 1) << std::endl;

            completed_processes ++;  
        }
    } while(completed_processes  < (num_nodes - 1));

    delete [] result_container;
    delete [] assigned_file_indices;

    // Save result
    std::vector<int> word_totals_counts;

    word_totals_counts.resize(dict_size);

    for (unsigned int i = 0; i < files_results.size(); i++) {
        std::cout << file_list.at(i) << std::endl;
        for (unsigned int j = 0; j < dict_size; j++) {
            std::cout << j << " " << files_results.at(i).at(j) << std::endl;
            word_totals_counts[j] += files_results.at(i).at(j);
        }
    }

    for (unsigned int i = 0; i < word_totals_counts.size(); i++) {
        std::cout << "Word" << i << " occurrences " << word_totals_counts.at(i) << std::endl;
    }

    std::ofstream outputFile;
    outputFile.open("result.txt");
    for (unsigned int i = 0; i < files_results.size(); i++) {
        outputFile << file_list.at(i) << std::endl;
        for (unsigned int j = 0; j < dict_size; j++) {
            outputFile << files_results.at(i).at(j) << std::endl;
        }
    }
    outputFile.close();
}

void worker(int argc, char* argv[], MPI_Comm workerCommunicator) {
    long int dict_size;
    int file_name_len;
    int worker_communicator_rank;

    MPI_Request pending;
    MPI_Status status;

    char* dict_encoded_chars;
    std::string dict_encoded;
    std::vector<std::string> items_dict;
    std::map<std::string, int> result_single_file;
    int* value_vector_pointer;
    std::vector<int> values_vector_data;

    MPI_Comm_rank(workerCommunicator, &worker_communicator_rank);

    // Ready to accept work
    MPI_Isend(nullptr, 0, MPI_UNSIGNED_CHAR, 0, Tags::EMPTY_MESSAGE, MPI_COMM_WORLD, &pending);

    if (worker_communicator_rank == 0) {
        // Worker 0 reads the dictionary
        std::ifstream dict_stream;

        dict_stream.open("data/checkedWords.txt", std::ifstream::in);

        for (std::string word; std::getline(dict_stream, word); ) {
            items_dict.push_back(word);
            dict_encoded += word += "|";
        }

        dict_encoded.erase(dict_encoded.size() - 1);

        dict_size = static_cast<long int>(dict_encoded.size());

        dict_encoded_chars = new char[dict_size];
        std::strcpy(dict_encoded_chars, dict_encoded.c_str());
    }

    MPI_Bcast(&dict_size, 1, MPI_LONG, 0, workerCommunicator);

    if (worker_communicator_rank != 0) {
        dict_encoded_chars = new char[dict_size];
    }

    MPI_Bcast(dict_encoded_chars, static_cast<int>(dict_size), MPI_CHAR, 0, workerCommunicator);

    if (worker_communicator_rank != 0) {
       // Build dictItems from the message

        std::string dict_item;
        std::stringstream sStream(dict_encoded_chars);

        while(std::getline(sStream, dict_item, '|')) {
            items_dict.push_back(dict_item);
        }
    }

    dict_size = static_cast<int>(items_dict.size());

    delete [] dict_encoded_chars;

    if (worker_communicator_rank == 0) {
        // Send dictionary size to the manager
        MPI_Send(&dict_size, 1, MPI_INT, 0, Tags::DICTIONARY_SIZE_MESSAGE, MPI_COMM_WORLD);
    }

    while (true) {
        MPI_Probe(0, Tags::SOURCE_FILE_MESSAGE, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_CHAR, &file_name_len);

        if (file_name_len == 0) {
            // No more work to do - terminate
            break;
        }

        std::string fileNameToRead;
        fileNameToRead.resize(file_name_len);

        MPI_Recv(fileNameToRead.data(), static_cast<int>(fileNameToRead.size()), MPI_CHAR, 0, Tags::SOURCE_FILE_MESSAGE, MPI_COMM_WORLD, &status);

        std::string word;
        std::ifstream readFile;

        readFile.open(fileNameToRead);

        while (readFile >> word) {
            ++result_single_file[word];
        }

        for (auto & dictWord : items_dict) {
            int value = 0;
            try {
                value = result_single_file.at(dictWord);
            } catch (std::exception& e) {}
            values_vector_data.push_back(value);
        }

        value_vector_pointer = new int[values_vector_data.size()];
        std::copy(values_vector_data.begin(), values_vector_data.end(), value_vector_pointer);

        MPI_Send(value_vector_pointer, static_cast<int>(dict_size), MPI_INT, 0, Tags::FILE_VECTOR_MESSAGE, MPI_COMM_WORLD);

        delete [] value_vector_pointer;

        values_vector_data.clear();
        result_single_file.clear();
    }

    MPI_Cancel(&pending);
    MPI_Request_free(&pending);
    MPI_Comm_free(&workerCommunicator);
}

int main(int argc, char* argv[]) {
    int num_processes;
    int process_id;
    MPI_Comm worker_communicator;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &num_processes);
    MPI_Comm_rank(MPI_COMM_WORLD, &process_id);

    if (!process_id) {
        MPI_Comm_split(MPI_COMM_WORLD, MPI_UNDEFINED, process_id, &worker_communicator);
        manager(argc, argv, num_processes);
    } else {
        MPI_Comm_split(MPI_COMM_WORLD, 0, process_id, &worker_communicator);
        worker(argc, argv, worker_communicator);
    }

    MPI_Finalize();
    return 0;
}
