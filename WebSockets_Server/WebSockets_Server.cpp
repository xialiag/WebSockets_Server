//#include <iostream>
#include <uwebsockets/App.h>
#include <map>
#include <json/reader.h>
#include <fstream>
#include <string>
#include <windows.h>
#include <cstdio>
using namespace std;
////////// V E R S I O N //////////
std::string _VER = "1.6.34";
std::string TitleCmd = "title WebSockets_Server-v" + _VER;
////////// I N C L U D E S //////////

Json::Reader reader;
Json::Value root;

////////// J S O N //////////

unsigned int port;
std::string passwd;
bool DevMode;//Show Message On Receive

////////// O P I N I O N S //////////

//User information
struct PerSocketData {
	string serverName;
	unsigned int server_id;
};

map<unsigned int, string> serverNames;

const string BROADCAST_CHANNEL = "broadcast";
const string MESSAGE_TO = "MESSAGE_TO::";
const string SET_NAME = "SET_NAME::";
const string OFFLINE = "OFFLINE::";
const string ONLINE = "ONLINE::";

inline string online(PerSocketData* serverData) {
	//check that there is such a user_id in the map!!!
	return ONLINE + serverData->serverName + '#' + to_string(serverData->server_id);
}

inline string offline(PerSocketData* serverData) {
	//check that there is such a user_id in the map!!!
	return OFFLINE + serverData->serverName + '#' + to_string(serverData->server_id);
}

void updateName(PerSocketData* data) {
	serverNames[data->server_id] = data->serverName;
}

void deleteServer(PerSocketData* data) {
	serverNames.erase(data->server_id);
}

bool isBroadcast(string message) {
	return message.find("BROADCAST::") == 0;
}

bool isGetConnections(string message) {
	return message.find("GET_CONNECTIONS") == 0;
}

bool isGetServers(string message) {
	return message.find("GET_SERVERS") == 0;
}

bool isSetName(string message) {
	return message.find(SET_NAME) == 0;
}

bool isMessageTo(string message) {
	return message.find(MESSAGE_TO) == 0;
}

string parseName(string message) {
	string rest = message.substr(message.find("::") + 2);
	//Incoming example: SET_NAME::Oblivion
	if (isSetName(message)) {
		return rest;
	}
	//Incoming example: MESSAGE_TO::Oblivion#1::{"type": "HeartBeat"}
	return rest.substr(0, rest.find("#"));

	//Return example: Oblivion
}

//getting user id

string parseServerId(string message) {
	//Incoming example: MESSAGE_TO::Oblivion#1::{"type": "HeartBeat"}
	string rest = message.substr(message.find("#") + 1);
	return rest.substr(0, rest.find("::"));

	//Return example: 1
}


string parseServerChannal(string message) {
	//Incoming example: MESSAGE_TO::Oblivion#1::{"type": "HeartBeat"}
	string rest = message.substr(message.find("::") + 2);
	return rest.substr(0, rest.find("::"));

	//Return example: Oblivion#1
}


/*getting user message*/
string parseServerText(string message) {
	string findParse = message.substr(message.find("::") + 2);
	//Incoming example: BROADCAST::{"type": "HeartBeat"} 
	if (isBroadcast(message)) {
		return findParse;
	}

	//Incoming example: MESSAGE_TO::Oblivion#1::{"type": "HeartBeat"}
	return findParse.substr(findParse.find("::") + 2);

	//Return example: {"type": "HeartBeat"}
}


string messageFrom(string server_id, string senderName, string message) {
	return "MESSAGE_FROM::" + senderName + '#' + server_id +"::" + message;
}

int main() {
	system("@echo off");//shield command output
	system(TitleCmd.c_str());
	system("chcp>nul 2>nul 65001");//shield "Active code page: 65001"

	std::string configName = "Config.json";
	std::ifstream configFile(configName.c_str());
	
	printf("Reading Config.json...\n");
	if (!configFile) {
	printf("Error!! Failed to read configuration file!\n");
	system("pause");
	return -1;
	}

	if (reader.parse(configFile, root)) {
	port = static_cast<unsigned int>(root["port"].asUInt());
	passwd = static_cast<string>(root["passwd"].asString());
	DevMode = static_cast<bool>(root["DevMode"].asBool());
	}


	printf("Configuration:\nPort: %s\nPasswd: %s\nDevMode: %s\n", std::to_string(port).c_str(), passwd.c_str(), std::to_string(DevMode).c_str());
	//readin config JSON file 


	unsigned int last_server_id = 0;

	/*Server configuration*/
	uWS::App(). //create a Server without encryptio
		ws<PerSocketData>("/" + passwd, {
		/* Settings */
		.compression = uWS::CompressOptions(uWS::DEDICATED_COMPRESSOR | uWS::DEDICATED_DECOMPRESSOR),
		//.compression = uWS::SHARED_COMPRESSOR,
		.maxPayloadLength = unsigned (-1),
		.idleTimeout = 1200, //sec
		.maxBackpressure = unsigned (-1),
		.closeOnBackpressureLimit = false,
		.resetIdleTimeoutOnSend = true,
		.sendPingsAutomatically = true,

			.open = [&last_server_id](auto* ws) {
				PerSocketData* serverData = (PerSocketData*)ws->getUserData();
				serverData->serverName = "UNNAMED";
				serverData->server_id = last_server_id++;

				//for (auto entry : serverNames) {
				//	ws->send(online(entry.first), uWS::OpCode::TEXT);
				//}

				updateName(serverData);
				//ws->publish(BROADCAST_CHANNEL, online(serverData->server_id), uWS::OpCode::TEXT);

				printf("\nNew server connected, id = %s\n", std::to_string(serverData->server_id).c_str());
				printf("Total server connected: %s\n", std::to_string(serverNames.size()).c_str());

				string server_channel = serverData->serverName + "#" + to_string(serverData->server_id);
				
				ws->subscribe(server_channel); //personal channel
				ws->subscribe(BROADCAST_CHANNEL); //common channel
			},
			.message = [&last_server_id](auto* ws, string_view message, uWS::OpCode opCode) {
				string strMessage = string(message);
				PerSocketData* serverData = (PerSocketData*)ws->getUserData();
				string authorId = to_string(serverData->server_id);

				if (DevMode) {
					printf("\nReceived message: %s\n", strMessage.c_str());
				}

				if (isMessageTo(strMessage)) {
					string receiverId = parseServerId(strMessage);
					if (stoi(receiverId) <= last_server_id && stoi(receiverId) >= 0) {
						string text = parseServerText(strMessage);
						string TargetChannal = parseServerChannal(strMessage);
						bool cont = false;
						for (auto i : serverNames) {
							if (TargetChannal == (i.second + '#' + to_string(i.first))) {
								cont = true;
							}
						}
						if (cont) {
							ws->publish(TargetChannal, messageFrom(authorId, serverData->serverName, text), uWS::OpCode::TEXT); //sending a message to the recipient
							printf("\nServer %s#%s send message to %s\n", serverData->serverName.c_str(), authorId.c_str(), TargetChannal.c_str());
						}
						else {
							printf("\nERROR::NO_SUCH_CHANNAL\n");
							ws->send("ERROR::NO_SUCH_CHANNAL", uWS::OpCode::TEXT);
						}
					}
					else {
						printf("\nERROR::NO_SUCH_CHANNAL\n");
						ws->send("ERROR::NO_SUCH_CHANNAL", uWS::OpCode::TEXT);
					}
				}
				if (isSetName(strMessage)) {
					string newName = parseName(strMessage);
					bool duplicate = false;

					if ((newName.find("::") == std::string::npos) && (newName.find("#") == std::string::npos)) {
						if (newName.length() <= 255) {

							for (auto i : serverNames) {
								if (newName == i.second) {
									printf("\nERROR::DUPLICATE_NAME\n");
									ws->send("ERROR::DUPLICATE_NAME", uWS::OpCode::TEXT);
									duplicate = true;
								}
							}

							if (!duplicate) {
								ws->unsubscribe(serverData->serverName + '#' + to_string(serverData->server_id));
								printf("\n%s#%s set name as %s\n", serverData->serverName.c_str(), authorId.c_str(), newName.c_str());
								serverData->serverName = newName;
								updateName(serverData);
								ws->subscribe(serverData->serverName + '#' + to_string(serverData->server_id));
								ws->publish(BROADCAST_CHANNEL, online(serverData), uWS::OpCode::TEXT);
							}
						}
						else {
							printf("\nERROR::NAME_TOO_LONG\n");
							ws->send("ERROR::NAME_TOO_LONG", uWS::OpCode::TEXT);
						}
					}
					else {
						printf("\nERROR::INVAILD_NAME\n");
						ws->send("ERROR::INVAILD_NAME", uWS::OpCode::TEXT);
					}

					
				}
				if (isGetServers(strMessage)) {
					string text = "[";
					for (auto i : serverNames) {
						text = text + "[\"" + i.second + "\", " + to_string(i.first) + "],";
					}
					text.erase(text.end() - 1);
					text += ']';
					printf("\nSending servers...\n");
					ws->send("SERVERS::" + text, uWS::OpCode::TEXT);
					//Return example:
					//[
					//  ["Oblivion", 1],
					//  ["Survice", 2],
					//  ["UNKNOW_SERVER", 3]
					//]
				}
				if (isGetConnections(strMessage)) {
					printf("\nSending connections...\n");
					ws->send("CONNECTIONS::" + to_string(serverNames.size()), uWS::OpCode::TEXT);
				}
				if (isBroadcast(strMessage)) {
					//Incoming example: BROADCAST::{"type": "HeartBeat"}
					printf("\nSending message to broadcast channal...\n");
					ws->publish(BROADCAST_CHANNEL, messageFrom(authorId, serverData->serverName, parseServerText(strMessage)), uWS::OpCode::TEXT);
				}
			},
			.close = [](auto* ws, int code, string_view message) {
				PerSocketData* serverData = (PerSocketData*)ws->getUserData();
				ws->publish(BROADCAST_CHANNEL, offline(serverData), uWS::OpCode::TEXT);
				printf("\nServer %s#%s disconnected\n", serverData->serverName.c_str(), to_string(serverData->server_id).c_str());
				printf("Total server connected: %s\n", std::to_string(serverNames.size()).c_str());
				deleteServer(serverData);
			}
			})
		/*Server start*/
				.listen(port, [](auto* listen_socket) {
				if (listen_socket) {
					printf("Listening on port %s\n", std::to_string(port).c_str());
				}
					}).run();  //Launch
}
