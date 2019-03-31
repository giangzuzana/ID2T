#include "statistics_db.h"
#include <math.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <numeric>
#include <unistd.h>
#include <stdio.h>
#include <pybind11/pybind11.h>
namespace py = pybind11;

using namespace Tins;

/**
 * Creates a new statistics_db object. Opens an existing database located at database_path. If not existing, creates
 * a new database at database_path.
 * @param database_path The file path of the database.
 */
statistics_db::statistics_db(std::string database_path, std::string resourcePath) {
    // Append file extension if not present
    if (database_path.find(".sqlite3") == database_path.npos) {
        database_path += ".sqlite3";
    }
    // creates the DB if not existing, opens the DB for read+write access
    db.reset(new SQLite::Database(database_path, SQLite::OPEN_CREATE | SQLite::OPEN_READWRITE));

    this->resourcePath = resourcePath;

    // Read ports and services into portServices vector
    readPortServicesFromNmap();
}

void statistics_db::getNoneExtraTestsInveralStats(std::vector<double>& intervals){
    try {
        //SQLite::Statement query(*db, "SELECT name FROM sqlite_master WHERE type='table' AND name='interval_tables';");
        std::vector<std::string> tables;
        try {
            SQLite::Statement query(*db, "SELECT name FROM interval_tables WHERE extra_tests=1;");
            while (query.executeStep()) {
                tables.push_back(query.getColumn(0));
            }
        } catch (std::exception &e) {
        }
        if (tables.size() != 0) {
            std::string table_name;
            double interval;
            for (auto table = tables.begin(); table != tables.end(); table++) {
                table_name = table->substr(std::string("interval_statistics_").length());
                interval = static_cast<double>(::atof(table_name.c_str()))/1000000;
                auto found = std::find(intervals.begin(), intervals.end(), interval);
                if (found != intervals.end()) {
                    intervals.erase(found, found);
                }
            }
        }
    } catch (std::exception &e) {
        std::cerr << "Exception in statistics_db::" << __func__ << ": " << e.what() << std::endl;
    }
}

/**
 * Writes the IP statistics into the database.
 * @param ipStatistics The IP statistics from class statistics.
 */
void statistics_db::writeStatisticsIP(const std::unordered_map<std::string, entry_ipStat> &ipStatistics) {
    try {
        db->exec("DROP TABLE IF EXISTS ip_statistics");
        SQLite::Transaction transaction(*db);
        const char *createTable = "CREATE TABLE ip_statistics ( "
                "ipAddress TEXT, "
                "pktsReceived INTEGER, "
                "pktsSent INTEGER, "
                "kbytesReceived REAL, "
                "kbytesSent REAL, "
                "maxPktRate REAL,"
                "minPktRate REAL,"
                "maxKByteRate REAL,"
                "minKByteRate REAL,"
                "maxLatency INTEGER,"
                "minLatency INTEGER,"
                "avgLatency INTEGER,"
                "ipClass TEXT COLLATE NOCASE, "
                "PRIMARY KEY(ipAddress));";
        db->exec(createTable);
        SQLite::Statement query(*db, "INSERT INTO ip_statistics VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        for (auto it = ipStatistics.begin(); it != ipStatistics.end(); ++it) {
            const entry_ipStat &e = it->second;
            int minDelay;
            int maxDelay;
            std::chrono::microseconds avgDelay;
            calculate_latency(&e.interarrival_times, &maxDelay, &minDelay, &avgDelay);
            query.bindNoCopy(1, it->first);
            query.bind(2, (int) e.pkts_received);
            query.bind(3, (int) e.pkts_sent);
            query.bind(4, e.kbytes_received);
            query.bind(5, e.kbytes_sent);
            query.bind(6, e.max_interval_pkt_rate);
            query.bind(7, e.min_interval_pkt_rate);
            query.bind(8, e.max_interval_kybte_rate);
            query.bind(9, e.min_interval_kybte_rate);
            query.bind(10, maxDelay);
            query.bind(11, minDelay);
            query.bind(12, static_cast<int>(avgDelay.count()));
            query.bindNoCopy(13, e.ip_class);
            query.exec();
            query.reset();

            if (PyErr_CheckSignals()) throw py::error_already_set();
        }
        transaction.commit();
    }
    catch (std::exception &e) {
        std::cerr << "Exception in statistics_db::" << __func__ << ": " << e.what() << std::endl;
    }
}

/**
 * Writes the IP Degrees into the database.
 * @param ipStatistics The IP statistics from class statistics. Degree Statistics are supposed to be integrated into the ip_statistics table later on,
 *        therefore they use the same parameter. But for now they are inserted into their own table.
 */
void statistics_db::writeStatisticsDegree(const std::unordered_map<std::string, entry_ipStat> &ipStatistics){
    try {
        db->exec("DROP TABLE IF EXISTS ip_degrees");
        SQLite::Transaction transaction(*db);
        const char *createTable = "CREATE TABLE ip_degrees ( "
                "ipAddress TEXT, "
                "inDegree INTEGER, "
                "outDegree INTEGER, "
                "overallDegree INTEGER, "
                "PRIMARY KEY(ipAddress));";
        db->exec(createTable);
        SQLite::Statement query(*db, "INSERT INTO ip_degrees VALUES (?, ?, ?, ?)");
        for (auto it = ipStatistics.begin(); it != ipStatistics.end(); ++it) {
            const entry_ipStat &e = it->second;
            query.bindNoCopy(1, it->first);
            query.bind(2, e.in_degree);
            query.bind(3, e.out_degree);
            query.bind(4, e.overall_degree);
            query.exec();
            query.reset();

            if (PyErr_CheckSignals()) throw py::error_already_set();
        }
        transaction.commit();
    }
    catch (std::exception &e) {
        std::cerr << "Exception in statistics_db::" << __func__ << ": " << e.what() << std::endl;
    }
}

/**
 * Writes the TTL distribution into the database.
 * @param ttlDistribution The TTL distribution from class statistics.
 */
void statistics_db::writeStatisticsTTL(const std::unordered_map<ipAddress_ttl, int> &ttlDistribution) {
    try {
        db->exec("DROP TABLE IF EXISTS ip_ttl");
        SQLite::Transaction transaction(*db);
        const char *createTable = "CREATE TABLE ip_ttl ("
                "ipAddress TEXT,"
                "ttlValue INTEGER,"
                "ttlCount INTEGER,"
                "PRIMARY KEY(ipAddress,ttlValue));"
                "CREATE INDEX ipAddressTTL ON ip_ttl(ipAddress);";
        db->exec(createTable);
        SQLite::Statement query(*db, "INSERT INTO ip_ttl VALUES (?, ?, ?)");
        for (auto it = ttlDistribution.begin(); it != ttlDistribution.end(); ++it) {
            const ipAddress_ttl &e = it->first;
            query.bindNoCopy(1, e.ipAddress);
            query.bind(2, e.ttlValue);
            query.bind(3, it->second);
            query.exec();
            query.reset();

            if (PyErr_CheckSignals()) throw py::error_already_set();
        }
        transaction.commit();
    }
    catch (std::exception &e) {
        std::cerr << "Exception in statistics_db::" << __func__ << ": " << e.what() << std::endl;
    }
}

/**
 * Writes the MSS distribution into the database.
 * @param mssDistribution The MSS distribution from class statistics.
 */
void statistics_db::writeStatisticsMSS(const std::unordered_map<ipAddress_mss, int> &mssDistribution) {
    try {
        db->exec("DROP TABLE IF EXISTS tcp_mss");
        SQLite::Transaction transaction(*db);
        const char *createTable = "CREATE TABLE tcp_mss ("
                "ipAddress TEXT,"
                "mssValue INTEGER,"
                "mssCount INTEGER,"
                "PRIMARY KEY(ipAddress,mssValue));"
                "CREATE INDEX ipAddressMSS ON tcp_mss(ipAddress);";
        db->exec(createTable);
        SQLite::Statement query(*db, "INSERT INTO tcp_mss VALUES (?, ?, ?)");
        for (auto it = mssDistribution.begin(); it != mssDistribution.end(); ++it) {
            const ipAddress_mss &e = it->first;
            query.bindNoCopy(1, e.ipAddress);
            query.bind(2, e.mssValue);
            query.bind(3, it->second);
            query.exec();
            query.reset();

            if (PyErr_CheckSignals()) throw py::error_already_set();
        }
        transaction.commit();
    }
    catch (std::exception &e) {
        std::cerr << "Exception in statistics_db::" << __func__ << ": " << e.what() << std::endl;
    }
}

/**
 * Writes the ToS distribution into the database.
 * @param tosDistribution The ToS distribution from class statistics.
 */
void statistics_db::writeStatisticsToS(const std::unordered_map<ipAddress_tos, int> &tosDistribution) {
    try {
        db->exec("DROP TABLE IF EXISTS ip_tos");
        SQLite::Transaction transaction(*db);
        const char *createTable = "CREATE TABLE ip_tos ("
                "ipAddress TEXT,"
                "tosValue INTEGER,"
                "tosCount INTEGER,"
                "PRIMARY KEY(ipAddress,tosValue));";
        db->exec(createTable);
        SQLite::Statement query(*db, "INSERT INTO ip_tos VALUES (?, ?, ?)");
        for (auto it = tosDistribution.begin(); it != tosDistribution.end(); ++it) {
            const ipAddress_tos &e = it->first;
            query.bindNoCopy(1, e.ipAddress);
            query.bind(2, e.tosValue);
            query.bind(3, it->second);
            query.exec();
            query.reset();

            if (PyErr_CheckSignals()) throw py::error_already_set();
        }
        transaction.commit();
    }
    catch (std::exception &e) {
        std::cerr << "Exception in statistics_db::" << __func__ << ": " << e.what() << std::endl;
    }
}

/**
 * Writes the window size distribution into the database.
 * @param winDistribution The window size distribution from class statistics.
 */
void statistics_db::writeStatisticsWin(const std::unordered_map<ipAddress_win, int> &winDistribution) {
    try {
        db->exec("DROP TABLE IF EXISTS tcp_win");
        SQLite::Transaction transaction(*db);
        const char *createTable = "CREATE TABLE tcp_win ("
                "ipAddress TEXT,"
                "winSize INTEGER,"
                "winCount INTEGER,"
                "PRIMARY KEY(ipAddress,winSize));"
                "CREATE INDEX ipAddressWIN ON tcp_win(ipAddress);";
        db->exec(createTable);
        SQLite::Statement query(*db, "INSERT INTO tcp_win VALUES (?, ?, ?)");
        for (auto it = winDistribution.begin(); it != winDistribution.end(); ++it) {
            const ipAddress_win &e = it->first;
            query.bindNoCopy(1, e.ipAddress);
            query.bind(2, e.winSize);
            query.bind(3, it->second);
            query.exec();
            query.reset();

            if (PyErr_CheckSignals()) throw py::error_already_set();
        }
        transaction.commit();
    }
    catch (std::exception &e) {
        std::cerr << "Exception in statistics_db::" << __func__ << ": " << e.what() << std::endl;
    }
}

/**
 * Writes the protocol distribution into the database.
 * @param protocolDistribution The protocol distribution from class statistics.
 */
void statistics_db::writeStatisticsProtocols(const std::unordered_map<ipAddress_protocol, entry_protocolStat> &protocolDistribution) {
    try {
        db->exec("DROP TABLE IF EXISTS ip_protocols");
        SQLite::Transaction transaction(*db);
        const char *createTable = "CREATE TABLE ip_protocols ("
                "ipAddress TEXT,"
                "protocolName TEXT COLLATE NOCASE,"
                "protocolCount INTEGER,"
                "byteCount REAL,"
                "PRIMARY KEY(ipAddress,protocolName));";
        db->exec(createTable);
        SQLite::Statement query(*db, "INSERT INTO ip_protocols VALUES (?, ?, ?, ?)");
        for (auto it = protocolDistribution.begin(); it != protocolDistribution.end(); ++it) {
            const ipAddress_protocol &e = it->first;
            query.bindNoCopy(1, e.ipAddress);
            query.bindNoCopy(2, e.protocol);
            query.bind(3, it->second.count);
            query.bind(4, it->second.byteCount);
            query.exec();
            query.reset();

            if (PyErr_CheckSignals()) throw py::error_already_set();
        }
        transaction.commit();
    }
    catch (std::exception &e) {
        std::cerr << "Exception in statistics_db::" << __func__ << ": " << e.what() << std::endl;
    }
}

/**
 * Writes the port statistics into the database.
 * @param portsStatistics The ports statistics from class statistics.
 */
void statistics_db::writeStatisticsPorts(const std::unordered_map<ipAddress_inOut_port, entry_portStat> &portsStatistics) {
    try {
        db->exec("DROP TABLE IF EXISTS ip_ports");
        SQLite::Transaction transaction(*db);
        const char *createTable = "CREATE TABLE ip_ports ("
                "ipAddress TEXT,"
                "portDirection TEXT COLLATE NOCASE,"
                "portNumber INTEGER,"
                "portCount INTEGER,"
                "byteCount REAL,"
                "portProtocol TEXT COLLATE NOCASE,"
                "portService TEXT COLLATE NOCASE,"
                "PRIMARY KEY(ipAddress,portDirection,portNumber,portProtocol));";
        db->exec(createTable);
        SQLite::Statement query(*db, "INSERT INTO ip_ports VALUES (?, ?, ?, ?, ?, ?, ?)");
        for (auto it = portsStatistics.begin(); it != portsStatistics.end(); ++it) {
            const ipAddress_inOut_port &e = it->first;

            std::string portService = portServices[e.portNumber];
            if(portService.empty()) {
                if(portServices[{0}] == "unavailable") {portService = "unavailable";}
                else {portService = "unknown";}
            }

            query.bindNoCopy(1, e.ipAddress);
            query.bindNoCopy(2, e.trafficDirection);
            query.bind(3, e.portNumber);
            query.bind(4, it->second.count);
            query.bind(5, it->second.byteCount);
            query.bindNoCopy(6, e.protocol);
            query.bindNoCopy(7, portService);
            query.exec();
            query.reset();

            if (PyErr_CheckSignals()) throw py::error_already_set();
        }
        transaction.commit();
    }
    catch (std::exception &e) {
        std::cerr << "Exception in statistics_db::" << __func__ << ": " << e.what() << std::endl;
    }
}

/**
 *  Writes the IP address -> MAC address mapping into the database.
 * @param IpMacStatistics The IP address -> MAC address mapping from class statistics.
 */
void statistics_db::writeStatisticsIpMac(const std::unordered_map<std::string, std::string> &IpMacStatistics) {
    try {
        db->exec("DROP TABLE IF EXISTS ip_mac");
        SQLite::Transaction transaction(*db);
        const char *createTable = "CREATE TABLE ip_mac ("
                "ipAddress TEXT,"
                "macAddress TEXT COLLATE NOCASE,"
                "PRIMARY KEY(ipAddress));";
        db->exec(createTable);
        SQLite::Statement query(*db, "INSERT INTO ip_mac VALUES (?, ?)");
        for (auto it = IpMacStatistics.begin(); it != IpMacStatistics.end(); ++it) {
            query.bindNoCopy(1, it->first);
            query.bindNoCopy(2, it->second);
            query.exec();
            query.reset();

            if (PyErr_CheckSignals()) throw py::error_already_set();
        }
        transaction.commit();
    }
    catch (std::exception &e) {
        std::cerr << "Exception in statistics_db::" << __func__ << ": " << e.what() << std::endl;
    }
}

/**
 * Writes general file statistics into the database.
 * @param packetCount The number of packets in the PCAP file.
 * @param captureDuration The duration of the capture (format: SS.mmmmmm).
 * @param timestampFirstPkt The timestamp of the first packet in the PCAP file.
 * @param timestampLastPkt The timestamp of the last packet in the PCAP file.
 * @param avgPacketRate The average packet rate (#packets / capture duration).
 * @param avgPacketSize The average packet size.
 * @param avgPacketsSentPerHost The average packets sent per host.
 * @param avgBandwidthIn The average incoming bandwidth.
 * @param avgBandwidthOut The average outgoing bandwidth.
 */
void statistics_db::writeStatisticsFile(int packetCount, float captureDuration, std::string timestampFirstPkt,
                                        std::string timestampLastPkt, float avgPacketRate, float avgPacketSize,
                                        float avgPacketsSentPerHost, float avgBandwidthIn, float avgBandwidthOut,
                                        bool doExtraTests) {
    try {
        db->exec("DROP TABLE IF EXISTS file_statistics");
        SQLite::Transaction transaction(*db);
        const char *createTable = "CREATE TABLE file_statistics ("
                "packetCount	INTEGER,"
                "captureDuration TEXT,"
                "timestampFirstPacket TEXT,"
                "timestampLastPacket TEXT,"
                "avgPacketRate REAL,"
                "avgPacketSize REAL,"
                "avgPacketsSentPerHost REAL,"
                "avgBandwidthIn REAL,"
                "avgBandwidthOut REAL,"
                "doExtraTests INTEGER);";
        db->exec(createTable);
        SQLite::Statement query(*db, "INSERT INTO file_statistics VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        query.bind(1, packetCount);
        query.bind(2, captureDuration);
        query.bind(3, timestampFirstPkt);
        query.bind(4, timestampLastPkt);
        query.bind(5, avgPacketRate);
        query.bind(6, avgPacketSize);
        query.bind(7, avgPacketsSentPerHost);
        query.bind(8, avgBandwidthIn);
        query.bind(9, avgBandwidthOut);
        query.bind(10, doExtraTests);
        query.exec();
        transaction.commit();
    }
    catch (std::exception &e) {
        std::cerr << "Exception in statistics_db::" << __func__ << ": " << e.what() << std::endl;
    }
}

/**
 * @brief statistics_db::calculate_latency This function
 * @param interarrival_times Pointer to vector containing inter arrival times.
 * @param sumLatency Pointer to sumLatency int.
 * @param maxLatency Pointer to maxLatency int.
 * @param minLatency Pointer to minLatency int.
 */
void statistics_db::calculate_latency(const std::vector<std::chrono::microseconds> *interarrival_times, int *maxLatency, int *minLatency, std::chrono::microseconds *avg_interarrival_time) {
    int sumLatency = 0;
    *minLatency = 0;
    *maxLatency = 0;
    int interTime = 0;
    for (auto iter = interarrival_times->begin(); iter != interarrival_times->end(); iter++) {
        interTime = static_cast<int>(iter->count());
        sumLatency += iter->count();
        if (*maxLatency < interTime)
            *maxLatency = interTime;
        if (*minLatency > interTime || *minLatency == 0)
            *minLatency = interTime;
    }
    if (interarrival_times->size() > 0) {
        *avg_interarrival_time = static_cast<std::chrono::microseconds>(sumLatency) / interarrival_times->size();
    } else {
        *avg_interarrival_time = static_cast<std::chrono::microseconds>(0);
    }
}

/**
 * Writes the conversation statistics into the database.
 * @param convStatistics The conversation from class statistics.
 */
void statistics_db::writeStatisticsConv(std::unordered_map<conv, entry_convStat> &convStatistics){
    try {
        db->exec("DROP TABLE IF EXISTS conv_statistics");
        SQLite::Transaction transaction(*db);
        const char *createTable = "CREATE TABLE conv_statistics ("
                "ipAddressA TEXT,"
                "portA INTEGER,"
                "ipAddressB TEXT,"
                "portB INTEGER,"
                "pktsCount INTEGER,"
                "avgPktRate REAL,"
                "avgDelay INTEGER,"
                "minDelay INTEGER,"
                "maxDelay INTEGER,"
                "roundTripTime INTEGER,"
                "PRIMARY KEY(ipAddressA,portA,ipAddressB,portB));";
        db->exec(createTable);
        SQLite::Statement query(*db, "INSERT INTO conv_statistics VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

        // Calculate average of inter-arrival times and average packet rate
        for (auto it = convStatistics.begin(); it != convStatistics.end(); ++it) {
            const conv &f = it->first;
            entry_convStat &e = it->second;
            if (e.pkts_count > 1){
                int minDelay;
                int maxDelay;
                calculate_latency(&e.interarrival_time, &maxDelay, &minDelay, &e.avg_interarrival_time);

                std::vector<std::chrono::microseconds>::const_iterator i1;
                std::vector<small_uint<12>>::const_iterator i2;
                std::chrono::microseconds roundTripTime = std::chrono::microseconds(0);
                std::vector<std::chrono::microseconds> roundTripTimes;
                bool flag = false;
                if (e.pkts_timestamp.size() != e.tcp_types.size()) {
                    std::cout << "shit..."<< std::endl;
                }
                for(i1 = e.pkts_timestamp.begin(), i2 = e.tcp_types.begin();
                    i1 < e.pkts_timestamp.end(), i2 < e.tcp_types.end();
                    ++i1, ++i2) {
                    if (*i2 == TCP::SYN && !flag) {
                        roundTripTime = *i1;
                        flag = true;
                    } else if (*i2 == TCP::ACK && flag) {
                        roundTripTime = *i1 - roundTripTime;
                        flag = false;
                        roundTripTimes.push_back(roundTripTime);
                        roundTripTime = std::chrono::microseconds(0);
                    }
                }

                if (roundTripTimes.size() != 0) {
                    std::vector<std::chrono::microseconds>::const_iterator it;
                    for(it = roundTripTimes.begin(); it < roundTripTimes.end(); ++it) {
                        roundTripTime += *it;
                    }
                    roundTripTime = roundTripTime / roundTripTimes.size();
                } else {
                    roundTripTime = std::chrono::microseconds(-1);
                }

                std::chrono::microseconds start_timesttamp = e.pkts_timestamp[0];
                std::chrono::microseconds end_timesttamp = e.pkts_timestamp.back();
                std::chrono::microseconds conn_duration = end_timesttamp - start_timesttamp;
                e.avg_pkt_rate = (float) e.pkts_count * 1000000 / conn_duration.count(); // pkt per sec

                query.bindNoCopy(1, f.ipAddressA);
                query.bind(2, f.portA);
                query.bindNoCopy(3, f.ipAddressB);
                query.bind(4, f.portB);
                query.bind(5, (int) e.pkts_count);
                query.bind(6, (float) e.avg_pkt_rate);
                query.bind(7, (int) e.avg_interarrival_time.count());
                query.bind(8, minDelay);
                query.bind(9, maxDelay);
                query.bind(10, static_cast<int>(roundTripTime.count()));
                query.exec();
                query.reset();

                if (PyErr_CheckSignals()) throw py::error_already_set();
            }
        }
        transaction.commit();
    }
    catch (std::exception &e) {
        std::cerr << "Exception in statistics_db::" << __func__ << ": " << e.what() << std::endl;
    }
}

/**
 * Writes the extended statistics for every conversation into the database.
 * @param conv_statistics_extended The extended conversation statistics from class statistics.
 */
void statistics_db::writeStatisticsConvExt(std::unordered_map<convWithProt, entry_convStatExt> &conv_statistics_extended){
    try {
        db->exec("DROP TABLE IF EXISTS conv_statistics_extended");
        SQLite::Transaction transaction(*db);
        const char *createTable = "CREATE TABLE conv_statistics_extended ("
                "ipAddressA TEXT,"
                "portA INTEGER,"
                "ipAddressB TEXT,"
                "portB INTEGER,"
                "protocol TEXT COLLATE NOCASE,"
                "pktsCount INTEGER,"
                "avgPktRate REAL,"
                "avgDelay INTEGER,"
                "minDelay INTEGER,"
                "maxDelay INTEGER,"
                "avgIntervalPktCount REAL,"
                "avgTimeBetweenIntervals REAL,"
                "avgIntervalTime REAL,"
                "totalConversationDuration REAL,"
                "PRIMARY KEY(ipAddressA,portA,ipAddressB,portB,protocol));";
        db->exec(createTable);
        SQLite::Statement query(*db, "INSERT INTO conv_statistics_extended VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        // iterate over every conversation and interval aggregation pair and store the respective values in the database
        for (auto it = conv_statistics_extended.begin(); it != conv_statistics_extended.end(); ++it) {
            const convWithProt &f = it->first;
            entry_convStatExt &e = it->second;

            int sumDelay = 0;
            int minDelay = -1;
            int maxDelay = -1;

            if (e.pkts_count > 1 && (f.protocol == "UDP" || f.protocol == "TCP")){
                for (int i = 0; (unsigned) i < e.interarrival_time.size(); i++) {
                    sumDelay += e.interarrival_time[i].count();
                    if (maxDelay < e.interarrival_time[i].count())
                        maxDelay = e.interarrival_time[i].count();
                    if (minDelay > e.interarrival_time[i].count() || minDelay == -1)
                        minDelay = e.interarrival_time[i].count();
                }
                if (e.interarrival_time.size() > 0)
                    e.avg_interarrival_time = static_cast<std::chrono::microseconds>(sumDelay) / e.interarrival_time.size(); // average
                else
                    e.avg_interarrival_time = static_cast<std::chrono::microseconds>(0);
            }

            if (e.total_comm_duration == 0)
                e.avg_pkt_rate = e.pkts_count; // pkt per sec
            else
                e.avg_pkt_rate = e.pkts_count / e.total_comm_duration;

            if (e.avg_int_pkts_count > 0){
                query.bindNoCopy(1, f.ipAddressA);
                query.bind(2, f.portA);
                query.bindNoCopy(3, f.ipAddressB);
                query.bind(4, f.portB);
                query.bindNoCopy(5, f.protocol);
                query.bind(6, static_cast<int>(e.pkts_count));
                query.bind(7, static_cast<float>(e.avg_pkt_rate));

                if ((f.protocol == "UDP" || f.protocol == "TCP") && e.pkts_count < 2)
                    query.bind(8);
                else
                    query.bind(8, abs(static_cast<int>(e.avg_interarrival_time.count())));

                if (minDelay == -1)
                    query.bind(9);
                else
                    query.bind(9, minDelay);

                if (maxDelay == -1)
                    query.bind(10);
                else
                    query.bind(10, maxDelay);

                query.bind(11, e.avg_int_pkts_count);
                query.bind(12, e.avg_time_between_ints);
                query.bind(13, e.avg_interval_time);
                query.bind(14, e.total_comm_duration);
                query.exec();
                query.reset();

                if (PyErr_CheckSignals()) throw py::error_already_set();
            }

        }
        transaction.commit();
    }
    catch (std::exception &e) {
        std::cerr << "Exception in statistics_db::" << __func__ << ": " << e.what() << std::endl;
    }
}

/**
 * Writes the interval statistics into the database.
 * @param intervalStatistics The interval entries from class statistics.
 */
void statistics_db::writeStatisticsInterval(const std::unordered_map<std::string, entry_intervalStat> &intervalStatistics, std::vector<std::chrono::duration<int, std::micro>> timeIntervals, bool del, int defaultInterval, bool extraTests){
    try {
        // remove old tables produced by prior database versions
        db->exec("DROP TABLE IF EXISTS interval_statistics");

        // delete all former interval statistics, if requested
        if (del) {
            SQLite::Statement query(*db, "SELECT name FROM sqlite_master WHERE type='table' AND name LIKE 'interval_statistics_%';");
            std::vector<std::string> previous_tables;
            while (query.executeStep()) {
                previous_tables.push_back(query.getColumn(0));
            }
            for (std::string table: previous_tables) {
                db->exec("DROP TABLE IF EXISTS " + table);
            }
            db->exec("DROP TABLE IF EXISTS interval_tables");
        }

        // create interval table index
        db->exec("CREATE TABLE IF NOT EXISTS interval_tables (name TEXT, is_default INTEGER, extra_tests INTEGER);");

        std::string default_table_name = "";
        // get name for default table
        try {
            SQLite::Statement query(*db, "SELECT name FROM interval_tables WHERE is_default=1;");
            query.executeStep();
            default_table_name = query.getColumn(0).getString();

        } catch (std::exception &e) {
        }

        // handle default interval only runs
        std::string is_default = "0";
        std::chrono::duration<int, std::micro> defaultTimeInterval(defaultInterval);
        if (defaultInterval != 0.0) {
            is_default = "1";
            if (timeIntervals.empty() || timeIntervals[0].count() == 0) {
                timeIntervals.clear();
                timeIntervals.push_back(defaultTimeInterval);
            }
        }

        // extra tests handling
        std::string extra = "0";
        if (extraTests) {
            extra = "1";
        }

        for (auto timeInterval: timeIntervals) {
            // get interval statistics table name
            std::ostringstream strs;
            strs << timeInterval.count();
            std::string table_name = "interval_statistics_" + strs.str();

            // check for recalculation of default table
            if (table_name == default_table_name || timeInterval == defaultTimeInterval) {
                is_default = "1";
            } else {
                is_default = "0";
            }

            // add interval_tables entry
            db->exec("DELETE FROM interval_tables WHERE name = '" + table_name + "';");
            db->exec("INSERT INTO interval_tables VALUES ('" + table_name + "', '" + is_default + "', '" + extra + "');");

            // new interval statistics implementation
            db->exec("DROP TABLE IF EXISTS " + table_name);
            SQLite::Transaction transaction(*db);
            db->exec("CREATE TABLE " + table_name + " ("
                    "last_pkt_timestamp TEXT,"
                    "first_pkt_timestamp TEXT,"
                    "pkts_count INTEGER,"
                    "pkt_rate REAL,"
                    "kBytes REAL,"
                    "kByte_rate REAL,"
                    "ip_src_entropy REAL,"
                    "ip_dst_entropy REAL,"
                    "ip_src_cum_entropy REAL,"
                    "ip_dst_cum_entropy REAL,"
                    "payload_count INTEGER,"
                    "incorrect_tcp_checksum_count INTEGER,"
                    "correct_tcp_checksum_count INTEGER,"
                    "ip_src_novel_Count INTEGER,"
                    "ip_dst_novel_Count INTEGER,"
                    "port_novel_count INTEGER,"
                    "ttl_novel_count INTEGER,"
                    "win_size_novel_count INTEGER,"
                    "tos_novel_count INTEGER,"
                    "mss_novel_count INTEGER,"
                    "port_entropy REAL,"
                    "ttl_entropy REAL,"
                    "win_size_entropy REAL,"
                    "tos_entropy REAL,"
                    "mss_entropy REAL,"
                    "port_novel_entropy REAL,"
                    "ttl_novel_entropy REAL,"
                    "win_size_novel_entropy REAL,"
                    "tos_novel_entropy REAL,"
                    "mss_novel_entropy REAL,"
                    "port_entropy_normalized REAL,"
                    "ttl_entropy_normalized REAL,"
                    "win_size_entropy_normalized REAL,"
                    "tos_entropy_normalized REAL,"
                    "mss_entropy_normalized REAL,"
                    "port_novel_entropy_normalized REAL,"
                    "ttl_novel_entropy_normalized REAL,"
                    "win_size_novel_entropy_normalized REAL,"
                    "tos_novel_entropy_normalized REAL,"
                    "mss_novel_entropy_normalized REAL,"
                    "ip_src_entropy_normalized REAL,"
                    "ip_dst_entropy_normalized REAL,"
                    "ip_src_cum_entropy_normalized REAL,"
                    "ip_dst_cum_entropy_normalized REAL,"
                    "ip_src_novel_entropy REAL,"
                    "ip_dst_novel_entropy REAL,"
                    "ip_src_novel_entropy_normalized REAL,"
                    "ip_dst_novel_entropy_normalized REAL,"
                    "PRIMARY KEY(last_pkt_timestamp));");

            SQLite::Statement query(*db, "INSERT INTO " + table_name + " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
            for (auto it = intervalStatistics.begin(); it != intervalStatistics.end(); ++it) {
                const entry_intervalStat &e = it->second;

                query.bindNoCopy(1, it->first);
                query.bind(2, e.start);
                query.bind(3, (int)e.pkts_count);
                query.bind(4, e.pkt_rate);
                query.bind(5, e.kbytes);
                query.bind(6, e.kbyte_rate);
                query.bind(7, e.ip_entropies[0]);
                query.bind(8, e.ip_entropies[1]);
                query.bind(9, e.ip_cum_entropies[0]);
                query.bind(10, e.ip_cum_entropies[1]);
                query.bind(11, e.payload_count);
                query.bind(12, e.incorrect_tcp_checksum_count);
                query.bind(13, e.correct_tcp_checksum_count);
                query.bind(14, static_cast<long long>(e.novel_ip_src_count));
                query.bind(15, static_cast<long long>(e.novel_ip_dst_count));
                query.bind(16, e.novel_port_count);
                query.bind(17, e.novel_ttl_count);
                query.bind(18, e.novel_win_size_count);
                query.bind(19, e.novel_tos_count);
                query.bind(20, e.novel_mss_count);
                query.bind(21, e.port_entropies[0]);
                query.bind(22, e.ttl_entropies[0]);
                query.bind(23, e.win_size_entropies[0]);
                query.bind(24, e.tos_entropies[0]);
                query.bind(25, e.mss_entropies[0]);
                query.bind(26, e.port_entropies[1]);
                query.bind(27, e.ttl_entropies[1]);
                query.bind(28, e.win_size_entropies[1]);
                query.bind(29, e.tos_entropies[1]);
                query.bind(30, e.mss_entropies[1]);
                query.bind(31, e.port_entropies[2]);
                query.bind(32, e.ttl_entropies[2]);
                query.bind(33, e.win_size_entropies[2]);
                query.bind(34, e.tos_entropies[2]);
                query.bind(35, e.mss_entropies[2]);
                query.bind(36, e.port_entropies[3]);
                query.bind(37, e.ttl_entropies[3]);
                query.bind(38, e.win_size_entropies[3]);
                query.bind(39, e.tos_entropies[3]);
                query.bind(40, e.mss_entropies[3]);
                query.bind(41, e.ip_entropies[4]);
                query.bind(42, e.ip_entropies[5]);
                query.bind(43, e.ip_cum_entropies[2]);
                query.bind(44, e.ip_cum_entropies[3]);
                query.bind(45, e.ip_entropies[2]);
                query.bind(46, e.ip_entropies[3]);
                query.bind(47, e.ip_entropies[6]);
                query.bind(48, e.ip_entropies[7]);
                query.exec();
                query.reset();

                if (PyErr_CheckSignals()) throw py::error_already_set();
            }
            transaction.commit();
        }
    }
    catch (std::exception &e) {
        std::cerr << "Exception in statistics_db::" << __func__ << ": " << e.what() << std::endl;
    }
}

void statistics_db::writeDbVersion(){
    try {
        SQLite::Transaction transaction(*db);
        SQLite::Statement query(*db, std::string("PRAGMA user_version = ") + std::to_string(DB_VERSION) + ";");
        query.exec();
        transaction.commit();
    }
    catch (std::exception &e) {
        std::cerr << "Exception in statistics_db::" << __func__ << ": " << e.what() << std::endl;
    }
}

/**
 * Reads all ports and their corresponding services from nmap-services-tcp.csv and stores them into portServices vector.
 */
void statistics_db::readPortServicesFromNmap()
{
    std::string portnumber;
    std::string service;
    std::string dump;
    std::string nmapPath = resourcePath + "nmap-services-tcp.csv";
    std::ifstream reader;

    reader.open(nmapPath, std::ios::in);

    if(reader.is_open())
    {
        getline(reader, dump);

        while(!reader.eof())
        {
            getline(reader, portnumber, ',');
            getline(reader, service, ',');
            getline(reader, dump);
            if(!service.empty() && !portnumber.empty())
            {
                portServices.insert({std::stoi(portnumber), service});
            }
        }

        reader.close();
    }

    else
    {
        std::cerr << "WARNING: " << nmapPath << " could not be opened! PortServices can't be read!" << std::endl;
        portServices.insert({0, "unavailable"});
    }
}

/**
 * Writes the unrecognized PDUs into the database.
 * @param unrecognized_PDUs The unrecognized PDUs from class statistics.
 */
void statistics_db::writeStatisticsUnrecognizedPDUs(const std::unordered_map<unrecognized_PDU, unrecognized_PDU_stat>
                                                    &unrecognized_PDUs) {
    try {
        db->exec("DROP TABLE IF EXISTS unrecognized_pdus");
        SQLite::Transaction transaction(*db);
        const char *createTable = "CREATE TABLE unrecognized_pdus ("
                "srcMac TEXT COLLATE NOCASE,"
                "dstMac TEXT COLLATE NOCASE,"
                "etherType INTEGER,"
                "pktCount INTEGER,"
                "timestampLastOccurrence TEXT,"
                "PRIMARY KEY(srcMac,dstMac,etherType));";
        db->exec(createTable);
        SQLite::Statement query(*db, "INSERT INTO unrecognized_pdus VALUES (?, ?, ?, ?, ?)");
        for (auto it = unrecognized_PDUs.begin(); it != unrecognized_PDUs.end(); ++it) {
            const unrecognized_PDU &e = it->first;
            query.bindNoCopy(1, e.srcMacAddress);
            query.bindNoCopy(2, e.dstMacAddress);
            query.bind(3, e.typeNumber);
            query.bind(4, it->second.count);
            query.bindNoCopy(5, it->second.timestamp_last_occurrence);
            query.exec();
            query.reset();

            if (PyErr_CheckSignals()) throw py::error_already_set();
        }
        transaction.commit();
    }
    catch (std::exception &e) {
        std::cerr << "Exception in statistics_db::" << __func__ << ": " << e.what() << std::endl;
    }
}
