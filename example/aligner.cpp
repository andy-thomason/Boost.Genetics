

#include <fstream>
#include <random>
#include <thread>
#include <future>
#include <atomic>
#include <cstdint>

#include <boost/genetics/fasta.hpp>

#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>

/*void generate() {
    using namespace boost::genetics;

    fasta_file builder;
    builder.append_random("chr1", 1000000, 0x9bac7615+0);
    builder.append_random("chr2",  900000, 0x9bac7615+1);
    builder.append_random("chr3",  800000, 0x9bac7615+2);
    builder.append_random("chr4",  700000, 0x9bac7615+3);
    builder.append_random("chr5",  600000, 0x9bac7615+4);
    builder.append_random("chr6",  500000, 0x9bac7615+5);
    builder.append_random("chr7",  400000, 0x9bac7615+6);
    builder.append_random("chr8",  300000, 0x9bac7615+7);
    builder.append_random("chr9",  200000, 0x9bac7615+0);
    builder.make_index(10);

    writer sizer(nullptr, nullptr);
    builder.write_binary(sizer);

    std::vector<char> buffer(sizer.size());
    writer wr(buffer.data(), buffer.data() + sizer.size());
    builder.write_binary(wr);

    std::ofstream out_file(binary_filename, std::ios_base::binary);
    out_file.write(buffer.data(), sizer.size());
}

void generate_fastq(boost::genetics::mapped_fasta_file &ref) {
    using namespace boost::genetics;

    std::default_random_engine generator;
    std::uniform_int_distribution<size_t> loc_distribution(0, ref.size()-key_size);
    std::uniform_int_distribution<size_t> error_loc_distribution(0, key_size-1);
    std::uniform_int_distribution<size_t> error_prob_distribution(0, 99);
    std::uniform_int_distribution<int> error_value_distribution(0, 4);
    std::uniform_int_distribution<int> rev_comp_value_distribution(0, 1);
    std::uniform_int_distribution<int> phred_distribution('#', 'J');
    std::vector<fasta_result> result;

    std::ofstream fastq_file(fastq_filename);
    if (!fastq_file.good()) {
        std::cerr << "unable to create " << fastq_filename << "\n";
        return;
    }

    std::string phred(key_size, ' ');
    for (int i = 0; i != 1000000; ) {
        size_t locus = loc_distribution(generator);
        size_t error_prob = error_prob_distribution(generator);
        int rev_comp = rev_comp_value_distribution(generator);
        std::string key = ref.get_string().substr(locus, key_size, rev_comp != 0);
        if (std::count(key.begin(), key.end(), 'N') < 3) {
            // Inject up to two random errors or an 'N'
            if (error_prob < 10) {
                size_t error_loc = error_loc_distribution(generator);
                int error_value = error_value_distribution(generator);
                key[error_loc] = "ACGTN"[error_value];
                if (error_prob < 1) {
                    size_t error_loc = error_loc_distribution(generator);
                    int error_value = error_value_distribution(generator);
                    key[error_loc] = "ACGTN"[error_value];
                }
            }
            for (int j = 0; j != key_size; ++j) {
                phred[j] = phred_distribution(generator);
            }
            fastq_file << "@r" << i << "." << locus << "." << rev_comp << "\n" << key << "\n+\n" << phred << "\n";
            ++i;
        }
    }
}

//! Very basic aligner.
void align(std::ifstream &file, boost::genetics::mapped_fasta_file &ref) {
    using namespace boost::genetics;

    std::ofstream sam_file(sam_filename, std::ios_base::binary);

    char name[80];
    char key[key_size*2+2];
    char plus[8];
    char phred[key_size*2+2];
    std::vector<fasta_result> result;
    std::string rev;
    size_t num_multiple = 0;
    size_t num_unmatched = 0;
    size_t num_reads = 0;
    while(!file.eof()) {
        file.getline(name, sizeof(name));
        if (file.eof()) break;
        file.getline(key, sizeof(key));
        file.getline(plus, sizeof(plus));
        file.getline(phred, sizeof(phred));
        ref.find_inexact(result, key, max_distance, max_results, max_gap, false, true);

        num_unmatched += result.size() == 0 ? 1 : 0;
        num_multiple += result.size() >= 2 ? 1 : 0;
        num_reads++;

        if (result.size() == 0) {
            std::cerr << name << ", " << key << ", " << file.eof() << " unmatched\n";
        }

        for (size_t i = 0; i != result.size(); ++i) {
            fasta_result &r = result[i];
            const chromosome &c = ref.find_chromosome(r.location);

            // SAM flags
            // https://ppotato.files.wordpress.com/2010/08/sam_output.pdf
            int flags = 0;
            flags |= r.reverse_complement ? 0x10 : 0x00;
            flags |= i != 0 ? 0x100 : 0x000;
            sam_file
                << name + 1 << '\t'
                << flags << '\t'
                << c.name << '\t'
                << r.location - c.start + 1 << '\t'
                << '\n'
            ;
        }
    }
    std::cerr << num_unmatched << " unmatched\n";
    std::cerr << num_multiple << " multiple\n";
    std::cerr << num_reads << " reads\n";
}*/

void index(int argc, char **argv) {
    using namespace boost::program_options;
    using namespace boost::genetics;

    options_description desc("aligner index <file1.fa> <file2.fa> ... <-o index.bin>");
    desc.add_options()
        ("help", "produce help message")
        ("fasta-files", value<std::vector<std::string> >(), "input filename(s)")
        ("output-file,o", value<std::string>()->default_value("index.bin"), "output filename")
        ("num-index-chars,n", value<int>()->default_value(12), "number of chars in first stage index")
    ;

    positional_options_description pod;
    pod.add("fasta-files", -1);

    variables_map vm;
    command_line_parser clp(argc, argv);
    store(
        clp.options(desc).positional(pod).run(),
        vm
    );
    notify(vm);

    if (vm.count("help") || argc == 1) {
        std::cout << desc << "\n";
        return;
    }

    fasta_file builder;
    auto fa_files = vm["fasta-files"].as<std::vector<std::string> >();
    for (auto &f : fa_files) {
        std::cerr << f << "\n";
        builder.append(f);
    }

    if (builder.get_string().size() == 0) {
        throw std::runtime_error("no fa files or empty reference");
    }

    builder.make_index(vm["num-index-chars"].as<int>());

    writer sizer(nullptr, nullptr);
    builder.write_binary(sizer);

    std::vector<char> buffer(sizer.size());
    writer wr(buffer.data(), buffer.data() + sizer.size());
    builder.write_binary(wr);

    std::ofstream out_file(vm["output-file"].as<std::string>(), std::ios_base::binary);
    out_file.write(buffer.data(), sizer.size());
}

void align(int argc, char **argv) {
    using namespace boost::program_options;
    using namespace boost::genetics;
    using namespace boost::interprocess;

    options_description desc("aligner align <index.bin> <file1.fq> <file2.fq> <-o out.sam>");
    desc.add_options()
        ("help", "produce help message")
        ("index,i", value<std::string>(), "index file")
        ("fastq-files,q", value<std::vector<std::string> >(), "fastq files (2 max)")
        ("output-file,o", value<std::string>()->default_value("out.sam"), "output filename")
    ;

    positional_options_description pod;
    pod.add("index", 1);
    pod.add("fastq-files", 2);

    variables_map vm;
    command_line_parser clp(argc, argv);
    store(
        clp.options(desc).positional(pod).run(),
        vm
    );
    notify(vm);

    if (vm.count("help") || !vm.count("index") || argc == 1) {
        std::cout << desc << "\n";
        return;
    }


    file_mapping fm(vm["index"].as<std::string>().c_str(), read_only);
    mapped_region region(fm, read_only);
    char *p = (char*)region.get_address();
    char *end = p + region.get_size();
    mapper m(p, end);
    mapped_fasta_file ref(m);

    auto fq_filenames = vm["fastq-files"].as<std::vector<std::string> >();

    if (fq_filenames.size() != 1 && fq_filenames.size() != 2) {
        throw std::runtime_error("expected one or two FASTQ files");
    }

    std::vector<std::ifstream> read_files;
    for (size_t i = 0; i != fq_filenames.size(); ++i) {
        auto &f = fq_filenames[i];
        std::cerr << f << "\n";
        read_files.emplace_back(f, std::ios_base::binary);
    }

    int num_threads = 1;
    size_t num_segments = 32;
    std::atomic<size_t> segment;
    std::vector<std::thread> align_threads;
    std::mutex read_mutex;
    for (int i = 0; i != num_threads; ++i) {
        align_threads.emplace_back(
            [&]() {
                const size_t buffer_size = 0x1000;
                std::vector<std::array<char, buffer_size> > buffers(read_files.size());
                std::vector<std::vector<const char*> > keyss(read_files.size());
                std::vector<std::vector<const char*> > namess(read_files.size());
                //std::vector<size_t> ends(read_files.size());
                size_t max_reads = ~0;
                for(;;) {
                    std::unique_lock<std::mutex> lock(read_mutex);
                    for (size_t i = 0; i != read_files.size(); ++i)  {
                        auto &buffer = buffers[i];
                        auto &stream = read_files[i];
                        auto &keys = keyss[i];
                        auto &names = namess[i];
                        stream.read(buffer.data(), buffer_size);
                        size_t bytes = (size_t)stream.gcount();
                        bool is_last_block = stream.eof();
                        const char *p = buffer.data();
                        const char *end = buffer.data() + bytes;
                        keys.resize(0);
                        names.resize(0);
                        while (p != end) {
                            names.push_back(p);
                            while (p != end && *p != '\n') ++p;
                            p += p != end;
                            keys.push_back(p);
                            while (p != end && *p != '\n') ++p;
                            p += p != end;
                            while (p != end && *p != '\n') ++p;
                            p += p != end;
                            while (p != end && *p != '\n') ++p;
                            p += p != end;
                        }
                        if (is_last_block) names.push_back(end);
                        size_t num_reads = names.size() - 1;
                        max_reads = std::min(max_reads, names.size());
                    }

                    if (max_reads == 0) break;

                    for (size_t i = 0; i != read_files.size(); ++i)  {
                        auto &buffer = buffers[i];
                        auto &stream = read_files[i];
                        auto &names = namess[i];
                        size_t bytes = (size_t)stream.gcount();
                        const char *end = buffer.data() + bytes;
                        stream.seekg(names[max_reads] - end, std::ios_base::cur);
                        std::cerr << names[0] << "\n";
                    }

                    std::cerr << "max_reads = " << max_reads << "\n";
                }
            }
        );
    }

    for (int i = 0; i != num_threads; ++i) {
        align_threads[i].join();
    }
}

int main(int argc, char **argv) {
    try {
        if (argc >= 2) {
            if (!strcmp(argv[1], "index")) {
                index(argc-1, argv+1);
                return 0;
            } else if (!strcmp(argv[1], "align")) {
                align(argc-1, argv+1);
                return 0;
            } else if (!strcmp(argv[1], "generate")) {
                //generate(argc-1, argv+1);
                return 0;
            } else {
                std::cerr << "unknown function " << argv[1] << "\n";
                return 1;
            }
        } else {
            std::cerr << "Usage:\n";
            std::cerr << "  aligner index <file1.fa> <file2.fa> ... <-o index.bin>       (Generate Index)\n";
            std::cerr << "  aligner align <index.bin> <file1.fq> <file2.fq> <-o out.sam> (Align against index) \n";
            //std::cerr << "  aligner generate ...\n";
            std::cerr << "  aligner <index|align>                                        (Get help for each function)\n";
            return 1;
        }
    } catch (boost::program_options::unknown_option u) {
        std::cerr << "error: " << u.what() << "\n";
        return 1;
    } catch(std::runtime_error e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

