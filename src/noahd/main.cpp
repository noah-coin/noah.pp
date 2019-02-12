#include <belt.pp/global.hpp>
#include <belt.pp/log.hpp>

#include <mesh.pp/fileutility.hpp>
#include <mesh.pp/processutility.hpp>
#include <mesh.pp/cryptoutility.hpp>
#include <mesh.pp/log.hpp>
#include <mesh.pp/pid.hpp>
#include <mesh.pp/settings.hpp>

#include <publiq.pp/node.hpp>

#include <boost/program_options.hpp>
#include <boost/locale.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>

#include <memory>
#include <iostream>
#include <vector>
#include <sstream>
#include <exception>
#include <thread>
#include <functional>

#include <csignal>

using namespace BlockchainMessage;
namespace program_options = boost::program_options;

using std::unique_ptr;
using std::string;
using std::cout;
using std::endl;
using std::vector;
using std::runtime_error;

bool process_command_line(int argc, char** argv,
                          beltpp::ip_address& p2p_bind_to_address,
                          vector<beltpp::ip_address>& p2p_connect_to_addresses,
                          beltpp::ip_address& rpc_bind_to_address,
                          string& data_directory,
                          meshpp::private_key& pv_key,
                          bool& log_enabled);

string genesis_signed_block();

static bool g_termination_handled = false;
static publiqpp::node* g_pnode = nullptr;
void termination_handler(int /*signum*/)
{
    g_termination_handled = true;
    if (g_pnode)
        g_pnode->terminate();
}

class port2pid_helper
{
    using Loader = meshpp::file_locker<meshpp::file_loader<PidConfig::Port2PID,
                                                            &PidConfig::Port2PID::from_string,
                                                            &PidConfig::Port2PID::to_string>>;
public:
    port2pid_helper(boost::filesystem::path const& _path, unsigned short _port)
        : port(_port)
        , path(_path)
        , eptr()
    {
        Loader ob(path);
        auto res = ob->reserved_ports.insert(std::make_pair(port, meshpp::current_process_id()));

        if (false == res.second)
        {
            string error = "port: ";
            error += std::to_string(res.first->first);
            error += " is locked by pid: ";
            error += std::to_string(res.first->second);
            error += " as specified in: ";
            error += path.string();
            throw runtime_error(error);
        }

        ob.save();
    }
    port2pid_helper(port2pid_helper const&) = delete;
    port2pid_helper(port2pid_helper&&) = delete;
    ~port2pid_helper()
    {
        _commit();
    }

    void commit()
    {
        _commit();

        if (eptr)
            std::rethrow_exception(eptr);
    }
private:
    void _commit()
    {
        try
        {
            Loader ob(path);
            auto it = ob.as_const()->reserved_ports.find(port);
            if (it == ob.as_const()->reserved_ports.end())
            {
                string error = "cannot find own port: ";
                error += std::to_string(port);
                error += " specified in: ";
                error += path.string();
                throw runtime_error(error);
            }

            ob->reserved_ports.erase(it);
            ob.save();
        }
        catch (...)
        {
            eptr = std::current_exception();
        }
    }
    unsigned short port;
    boost::filesystem::path path;
    std::exception_ptr eptr;
};

template <typename NODE>
void loop(NODE& node, beltpp::ilog_ptr& plogger_exceptions, bool& termination_handled);

int main(int argc, char** argv)
{
    try
    {
        //  boost filesystem UTF-8 support
        std::locale::global(boost::locale::generator().generate(""));
        boost::filesystem::path::imbue(std::locale());
    }
    catch (...)
    {}  //  don't care for exception, for now
    //
    meshpp::config::set_public_key_prefix("NOAH");
    //
    meshpp::settings::set_application_name("noahd");
    meshpp::settings::set_data_directory(meshpp::config_directory_path().string());

    beltpp::ip_address p2p_bind_to_address;
    beltpp::ip_address rpc_bind_to_address;
    vector<beltpp::ip_address> p2p_connect_to_addresses;
    string data_directory;
    NodeType n_type = NodeType::miner;
    bool log_enabled = false;
    meshpp::random_seed seed;
    meshpp::private_key pv_key = seed.get_private_key(0);

    if (false == process_command_line(argc, argv,
                                      p2p_bind_to_address,
                                      p2p_connect_to_addresses,
                                      rpc_bind_to_address,
                                      data_directory,
                                      pv_key,
                                      log_enabled))
        return 1;

    if (false == data_directory.empty())
        meshpp::settings::set_data_directory(data_directory);

#ifdef B_OS_WINDOWS
    signal(SIGINT, termination_handler);
#else
    struct sigaction signal_handler;
    signal_handler.sa_handler = termination_handler;
    ::sigaction(SIGINT, &signal_handler, nullptr);
    ::sigaction(SIGTERM, &signal_handler, nullptr);
#endif

    beltpp::ilog_ptr plogger_exceptions = beltpp::t_unique_nullptr<beltpp::ilog>();
    beltpp::ilog_ptr plogger_storage_exceptions = beltpp::t_unique_nullptr<beltpp::ilog>();

    try
    {
        meshpp::create_config_directory();
        meshpp::create_data_directory();

        unique_ptr<port2pid_helper> port2pid(new port2pid_helper(meshpp::config_file_path("pid"), p2p_bind_to_address.local.port));

        using DataDirAttributeLoader = meshpp::file_locker<meshpp::file_loader<PidConfig::DataDirAttribute,
                                                                                &PidConfig::DataDirAttribute::from_string,
                                                                                &PidConfig::DataDirAttribute::to_string>>;
        DataDirAttributeLoader dda(meshpp::data_file_path("running.txt"));
        {
            PidConfig::RunningDuration item;
            item.start.tm = item.end.tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

            dda->history.push_back(item);
            dda.save();
        }

        auto fs_blockchain = meshpp::data_directory_path("blockchain");
        auto fs_action_log = meshpp::data_directory_path("action_log");
        auto fs_storage = meshpp::data_directory_path("storage");
        auto fs_transaction_pool = meshpp::data_directory_path("transaction_pool");
        auto fs_state = meshpp::data_directory_path("state");
        auto fs_log = meshpp::data_directory_path("log");

        cout << "p2p local address: " << p2p_bind_to_address.to_string() << endl;
        for (auto const& item : p2p_connect_to_addresses)
            cout << "p2p host: " << item.to_string() << endl;
        if (false == rpc_bind_to_address.local.empty())
            cout << "rpc interface: " << rpc_bind_to_address.to_string() << endl;

        beltpp::ilog_ptr plogger_p2p = beltpp::console_logger("noahd_p2p", false);
        plogger_p2p->disable();
        beltpp::ilog_ptr plogger_rpc = beltpp::console_logger("noahd_rpc", true);
        //plogger_rpc->disable();
        plogger_exceptions = meshpp::file_logger("noahd_exceptions",
                                                 fs_log / "exceptions.txt");
        plogger_storage_exceptions = meshpp::file_logger("storage_exceptions",
                                                         fs_log / "storage_exceptions.txt");

        //__debugbreak();
        
        publiqpp::node node(genesis_signed_block(),
                            rpc_bind_to_address,
                            p2p_bind_to_address,
                            p2p_connect_to_addresses,
                            fs_blockchain,
                            fs_action_log,
                            fs_transaction_pool,
                            fs_state,
                            plogger_p2p.get(),
                            plogger_rpc.get(),
                            pv_key,
                            n_type,
                            log_enabled);

        g_pnode = &node;

        cout << endl;
        cout << "Node: " << node.name() << endl;
        cout << "Type: " << static_cast<int>(n_type) << endl;
        cout << endl;

        loop(node, plogger_exceptions, g_termination_handled);

        dda->history.back().end.tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        dda.save();
        port2pid->commit();
    }
    catch (std::exception const& ex)
    {
        if (plogger_exceptions)
            plogger_exceptions->message(ex.what());
        cout << "exception cought: " << ex.what() << endl;
    }
    catch (...)
    {
        if (plogger_exceptions)
            plogger_exceptions->message("always throw std::exceptions");
        cout << "always throw std::exceptions" << endl;
    }
    return 0;
}

template <typename NODE>
void loop(NODE& node, beltpp::ilog_ptr& plogger_exceptions, bool& termination_handled)
{
    while (true)
    {
        try
        {
            if (termination_handled)
                break;
            if (false == node.run())
                break;
        }
        catch (std::bad_alloc const& ex)
        {
            if (plogger_exceptions)
                plogger_exceptions->message(ex.what());
            cout << "exception cought: " << ex.what() << endl;
            cout << "will exit now" << endl;
            termination_handler(0);
            break;
        }
        catch (std::exception const& ex)
        {
            if (plogger_exceptions)
                plogger_exceptions->message(ex.what());
            cout << "exception cought: " << ex.what() << endl;
        }
        catch (...)
        {
            if (plogger_exceptions)
                plogger_exceptions->message("always throw std::exceptions, will exit now");
            cout << "always throw std::exceptions, will exit now" << endl;
            termination_handler(0);
            break;
        }
    }
}

bool process_command_line(int argc, char** argv,
                          beltpp::ip_address& p2p_bind_to_address,
                          vector<beltpp::ip_address>& p2p_connect_to_addresses,
                          beltpp::ip_address& rpc_bind_to_address,
                          string& data_directory,
                          meshpp::private_key& pv_key,
                          bool& log_enabled)
{
    string p2p_local_interface;
    string rpc_local_interface;
    string str_pv_key;
    vector<string> hosts;
    program_options::options_description options_description;
    try
    {
        auto desc_init = options_description.add_options()
            ("help,h", "Print this help message and exit.")
            ("p2p_local_interface,i", program_options::value<string>(&p2p_local_interface)->required(),
                            "(p2p) The local network interface and port to bind to")
            ("p2p_remote_host,p", program_options::value<vector<string>>(&hosts),
                            "Remote nodes addresss with port")
            ("rpc_local_interface,r", program_options::value<string>(&rpc_local_interface),
                            "(rpc) The local network interface and port to bind to")
            ("data_directory,d", program_options::value<string>(&data_directory),
                            "Data directory path")
            ("node_private_key,k", program_options::value<string>(&str_pv_key),
                            "Node private key to start with");
        (void)(desc_init);

        program_options::variables_map options;

        program_options::store(
                    program_options::parse_command_line(argc, argv, options_description),
                    options);

        program_options::notify(options);

        if (options.count("help"))
        {
            throw std::runtime_error("");
        }

        p2p_bind_to_address.from_string(p2p_local_interface);
        if (false == rpc_local_interface.empty())
            rpc_bind_to_address.from_string(rpc_local_interface);

        for (auto const& item : hosts)
        {
            beltpp::ip_address address_item;
            address_item.from_string(item);
            p2p_connect_to_addresses.push_back(address_item);
        }

        if (p2p_connect_to_addresses.empty())
        {
            beltpp::ip_address address_item;
            address_item.from_string("noah.network:11177");
            p2p_connect_to_addresses.push_back(address_item);
        }

        if (false == str_pv_key.empty())
            pv_key = meshpp::private_key(str_pv_key);
        else
            log_enabled = true;
    }
    catch (std::exception const& ex)
    {
        std::stringstream ss;
        ss << options_description;

        string ex_message = ex.what();
        if (false == ex_message.empty())
            cout << ex.what() << endl << endl;
        cout << ss.str();
        return false;
    }
    catch (...)
    {
        cout << "always throw std::exceptions" << endl;
        return false;
    }

    return true;
}


string genesis_signed_block()
{
    return R"genesis({"rtt":6,"block_details":{"rtt":5,"header":{"rtt":4,"block_number":0,"delta":0,"c_sum":0,"c_const":1,"prev_hash":"GN41ATrkGn58hDMiW6eMkert3E23JnUvJ2mXcxWWiPyL","time_signed":"2019-02-08 09:10:37"},"rewards":[{"rtt":10,"to":"NOAH7Ta31VaxCB9VfDRvYYosKYpzxXNgVH46UkM9i4FhzNg4JEU3YJ","amount":{"rtt":0,"whole":100,"fraction":0},"reward_type":"initial"},{"rtt":10,"to":"NOAH76Zv5QceNSLibecnMGEKbKo3dVFV6HRuDSuX59mJewJxHPhLwu","amount":{"rtt":0,"whole":100,"fraction":0},"reward_type":"initial"},{"rtt":10,"to":"NOAH7JEFjtQNjyzwnThepF2jJtCe7cCpUFEaxGdUnN2W9wPP5Nh92G","amount":{"rtt":0,"whole":100,"fraction":0},"reward_type":"initial"},{"rtt":10,"to":"NOAH8f5Z8SKVrYFES1KLHtCYMx276a5NTgZX6baahzTqkzfnB4Pidk","amount":{"rtt":0,"whole":100,"fraction":0},"reward_type":"initial"},{"rtt":10,"to":"NOAH8MiwBdYzSj38etLYLES4FSuKJnLPkXAJv4MyrLW7YJNiPbh4z6","amount":{"rtt":0,"whole":100,"fraction":0},"reward_type":"initial"}],"signed_transactions":[]},"authority":"NOAH5HnbMEwb8AYsqZrrEwPaKZ1kzADmwuUMhhtdhL5ZdCCW5pkWmq","signature":"iKx1CJP9zUCfVn3kRkxmBcVkNPVNWJthK8ZnPRebB6y2RE1SNsbHXrzGtu9kbtTgsyQwxNx2SohgkH4UGFwNB7TbS3bf5ZwikT"})genesis";
}
