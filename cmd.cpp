/*
 * cmd.cpp: command handler functions and data connection manager
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <jjolano@me.com> wrote this file. As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return John Olano
 * ----------------------------------------------------------------------------
 */

#include <cstdio>
#include <ctime>
#include <map>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "ftp.h"
#include "defs.h"

using namespace std;

struct ftp_cvars {
	bool authorized;	// Client logged in?
	string cmd;			// Used in some commands like USER and PASS
	string cwd;			// Client current working directory
	string rnfr;		// Used for RNFR command
	u64 rest;			// Used in resuming file transfers
	s32 fd;				// Data file descriptor
	int type;			// Type of data transfer
	char* buffer;		// Used in data transfers
};

map<ftp_client*,ftp_cvars> client_cvar;
map<int,ftp_client*> data_client;

bool isClientAuthorized(ftp_client* clnt)
{
	map<ftp_client*,ftp_cvars>::iterator it = client_cvar.find(clnt);
	return (it != client_cvar.end() ? (it->second).authorized : false);
}

string getAbsPath(string cwd, string nd)
{
	if(nd[0] == '/')
	{
		// already absolute
		return nd;
	}

	if(cwd[cwd.size() - 1] != '/')
	{
		cwd += '/';
	}

	return cwd + nd;
}

bool fileExists(string path)
{
	sysFSStat stat;
	return (sysFsStat(path.c_str(), &stat) == 0);
}

bool isDirectory(string path)
{
	sysFSStat stat;
	return (sysFsStat(path.c_str(), &stat) == 0 && S_ISDIR(stat.st_mode));
}

vector<unsigned short> parsePORT(string args)
{
	vector<unsigned short> vec;
	stringstream ss(args);

	unsigned short i;

	while(ss >> i)
	{
		vec.push_back(i);

		if(ss.peek() == ',')
		{
			ss.ignore();
		}
	}

	return vec;
}

void data_list(int sock_data)
{

}

void data_mlsd(int sock_data)
{

}

void data_nlst(int sock_data)
{

}

void data_stor(int sock_data)
{

}

void data_retr(int sock_data)
{
	
}

void data_handler(int sock_data)
{
	ftp_client* clnt = data_client[sock_data];
	s32 fd = client_cvar[clnt].fd;
	int status = 0;

	switch(client_cvar[clnt].type)
	{
		case DATA_TYPE_LIST:
		{
			sysFSDirent entry;
			u64 read;

			if(sysFsReaddir(fd, &entry, &read) == 0 && read > 0)
			{
				string filename(entry.d_name);

				if(client_cvar[clnt].cwd == "/"
				   && (filename == "app_home" || filename == "host_root"))
				{
					// skip app_home and host_root since they lock up
					break;
				}

				string path = getAbsPath(client_cvar[clnt].cwd, filename);
				sysFSStat stat;
				sysFsStat(path.c_str(), &stat);

				char tstr[14];
				strftime(tstr, 13, "%3.3b %2e %H:%M", localtime(&stat.st_mtime));

				ostringstream permstr;
				permstr << (S_ISDIR(stat.st_mode) ? 'd' : '-');
				permstr << ((stat.st_mode & S_IRUSR) ? 'r' : '-');
				permstr << ((stat.st_mode & S_IWUSR) ? 'w' : '-');
				permstr << ((stat.st_mode & S_IXUSR) ? 'x' : '-');
				permstr << ((stat.st_mode & S_IRGRP) ? 'r' : '-');
				permstr << ((stat.st_mode & S_IWGRP) ? 'w' : '-');
				permstr << ((stat.st_mode & S_IXGRP) ? 'x' : '-');
				permstr << ((stat.st_mode & S_IROTH) ? 'r' : '-');
				permstr << ((stat.st_mode & S_IWOTH) ? 'w' : '-');
				permstr << ((stat.st_mode & S_IXOTH) ? 'x' : '-');

				int len = snprintf(client_cvar[clnt].buffer, DATA_BUFFER, "%s   1 %-10s %-10s %10lu %s %s\r\n", permstr.str().c_str(), "root", "root", stat.st_size, tstr, filename.c_str());

				send(sock_data, client_cvar[clnt].buffer, len, 0);
			}
			else
			{
				status = 1;
			}
			break;
		}
		case DATA_TYPE_MLSD:
		{
			sysFSDirent entry;
			u64 read;

			if(sysFsReaddir(fd, &entry, &read) == 0 && read > 0)
			{
				string filename(entry.d_name);

				if(client_cvar[clnt].cwd == "/"
				   && (filename == "app_home" || filename == "host_root"))
				{
					// skip app_home and host_root since they lock up
					break;
				}

				string path = getAbsPath(client_cvar[clnt].cwd, filename);
				sysFSStat stat;
				sysFsStat(path.c_str(), &stat);

				char tstr[15];
				strftime(tstr, 14, "%Y%m%d%H%M%S", localtime(&stat.st_mtime));

				string type("file");
				string typd("e");

				if(filename == ".")
				{
					type = "cdir";
				}
				else if(filename == "..")
				{
					type = "pdir";
				}

				if(S_ISDIR(stat.st_mode))
				{
					type = "dir";
					typd = "d";
				}

				ostringstream permstr;
				permstr << (((stat.st_mode & S_IRUSR) != 0) * 4 + ((stat.st_mode & S_IWUSR) != 0) * 2 + ((stat.st_mode & S_IXUSR) != 0) * 1);
				permstr << (((stat.st_mode & S_IRGRP) != 0) * 4 + ((stat.st_mode & S_IWGRP) != 0) * 2 + ((stat.st_mode & S_IXGRP) != 0) * 1);
				permstr << (((stat.st_mode & S_IROTH) != 0) * 4 + ((stat.st_mode & S_IWOTH) != 0) * 2 + ((stat.st_mode & S_IXOTH) != 0) * 1);

				int len = snprintf(client_cvar[clnt].buffer, DATA_BUFFER, "type=%s;siz%s=%lu;modify=%s;UNIX.mode=0%s;UNIX.uid=0;UNIX.gid=0;%s\r\n", type.c_str(), typd.c_str(), stat.st_size, tstr, permstr.str().c_str(), filename.c_str());

				send(sock_data, client_cvar[clnt].buffer, len, 0);
			}
			else
			{
				status = 1;
			}
			break;
		}
		case DATA_TYPE_NLST:
		{
			sysFSDirent entry;
			u64 read;

			if(sysFsReaddir(fd, &entry, &read) == 0 && read > 0)
			{
				size_t len = snprintf(client_cvar[clnt].buffer, DATA_BUFFER, "%s\r\n", entry.d_name);
				send(sock_data, client_cvar[clnt].buffer, len, 0);
			}
			else
			{
				status = 1;
			}
			break;
		}
		case DATA_TYPE_STOR:
		{
			u64 pos;
			u64 written;
			u64 read;

			if(client_cvar[clnt].rest > 0)
			{
				sysFsLseek(fd, (s64)client_cvar[clnt].rest, SEEK_SET, &pos);
				client_cvar[clnt].rest = 0;
			}

			read = (u64)recv(sock_data, client_cvar[clnt].buffer, DATA_BUFFER - 1, MSG_WAITALL);

			if(read > 0)
			{
				// data available, write to disk
				s32 ret = sysFsWrite(fd, client_cvar[clnt].buffer, read, &written);

				if(ret != 0 || written < read)
				{
					// write error
					status = -1;
				}
			}
			else
			{
				status = 1;
			}
			break;
		}
		case DATA_TYPE_RETR:
		{
			u64 pos;
			u64 read;

			if(client_cvar[clnt].rest > 0)
			{
				sysFsLseek(fd, (s64)client_cvar[clnt].rest, SEEK_SET, &pos);
				client_cvar[clnt].rest = 0;
			}

			s32 ret = sysFsRead(fd, client_cvar[clnt].buffer, DATA_BUFFER - 1, &read);

			if(ret == 0 && read > 0)
			{
				u64 sent = (u64)send(sock_data, client_cvar[clnt].buffer, (size_t)read, 0);

				if(sent < read)
				{
					// send error
					status = -1;
				}
			}
			else
			{
				status = 1;
			}
			break;
		}
		default:
			status = 2; // some random
	}

	if(status != 0)
	{
		closesocket(sock_data);
		delete [] client_cvar[clnt].buffer;
		clnt->sock_data = -1;

		switch(client_cvar[clnt].type)
		{
			case DATA_TYPE_LIST:
			case DATA_TYPE_MLSD:
			case DATA_TYPE_NLST:
				sysFsClosedir(client_cvar[clnt].fd);
				break;
			default:
				sysFsClose(client_cvar[clnt].fd);
		}

		client_cvar[clnt].fd = -1;

		switch(status)
		{
			case 1:
				clnt->response(226, "Transfer complete");
				break;
			case -1:
				clnt->response(451, "Data process error");
				break;
		}
	}
}

void cmd_success(ftp_client* clnt, string cmd, string args)
{
	clnt->response(200, cmd + " OK");
}

void cmd_success_auth(ftp_client* clnt, string cmd, string args)
{
	if(isClientAuthorized(clnt))
	{
		clnt->response(200, cmd + " OK");
	}
	else
	{
		clnt->response(530, "Not logged in");
	}
}

void cmd_ignored(ftp_client* clnt, string cmd, string args)
{
	clnt->response(202, cmd + " ignored");
}

void cmd_ignored_auth(ftp_client* clnt, string cmd, string args)
{
	if(isClientAuthorized(clnt))
	{
		clnt->response(202, cmd + " ignored");
	}
	else
	{
		clnt->response(530, "Not logged in");
	}
}

void cmd_syst(ftp_client* clnt, string cmd, string args)
{
	clnt->response(215, "UNIX Type: L8");
}

void cmd_quit(ftp_client* clnt, string cmd, string args)
{
	clnt->active = false;
	clnt->response(221, "Goodbye");
}

// cmd_feat: server feature list
void cmd_feat(ftp_client* clnt, string cmd, string args)
{
	vector<string> feat;

	feat.push_back("REST STREAM");
	feat.push_back("PASV");
	feat.push_back("PORT");
	feat.push_back("MDTM");
	feat.push_back("MLSD");
	feat.push_back("SIZE");
	feat.push_back("SITE CHMOD");
	feat.push_back("APPE");
	feat.push_back("MLST type*;size*;sizd*;modify*;UNIX.mode*;UNIX.uid*;UNIX.gid*;");

	clnt->control_sendCode(211, "Features:", true);

	for(vector<string>::iterator it = feat.begin(); it != feat.end(); it++)
	{
		clnt->control_sendCustom(" " + *it);
	}

	clnt->response(211, "End");

	feat.clear();
}

// cmd_user: this will initialize the client's cvars
void cmd_user(ftp_client* clnt, string cmd, string args)
{
	if(!isClientAuthorized(clnt))
	{
		client_cvar[clnt].authorized = false;
		client_cvar[clnt].cmd = "USER";

		clnt->response(331, "Username " + args + " OK. Password required");
	}
	else
	{
		clnt->response(230, "You are already logged in");
	}
}

// cmd_pass: this will verify username and password and set authorized flag
void cmd_pass(ftp_client* clnt, string cmd, string args)
{
	if(!isClientAuthorized(clnt))
	{
		if(client_cvar[clnt].cmd == "USER")
		{
			// set authorized flag
			client_cvar[clnt].authorized = true;

			// set cwd to root
			client_cvar[clnt].cwd = "/";

			client_cvar[clnt].fd = -1;

			clnt->response(230, "Successfully logged in");
		}
		else
		{
			clnt->response(503, "Bad command sequence");
		}
	}
	else
	{
		clnt->response(230, "You are already logged in");
	}
}

void cmd_cwd(ftp_client* clnt, string cmd, string args)
{
	if(isClientAuthorized(clnt))
	{
		string path = getAbsPath(client_cvar[clnt].cwd, args);

		if(isDirectory(path))
		{
			client_cvar[clnt].cwd = path;
			clnt->response(250, "Directory change successful");
		}
		else
		{
			clnt->response(550, "Cannot access directory");
		}
	}
	else
	{
		clnt->response(530, "Not logged in");
	}
}

void cmd_pwd(ftp_client* clnt, string cmd, string args)
{
	if(isClientAuthorized(clnt))
	{
		clnt->response(257, "\"" + client_cvar[clnt].cwd + "\" is the current directory");
	}
	else
	{
		clnt->response(530, "Not logged in");
	}
}

void cmd_mkd(ftp_client* clnt, string cmd, string args)
{
	if(isClientAuthorized(clnt))
	{
		string path = getAbsPath(client_cvar[clnt].cwd, args);

		if(sysFsMkdir(path.c_str(), 755) == 0)
		{
			clnt->response(257, "\"" + args + "\" was successfully created");
		}
		else
		{
			clnt->response(550, "Cannot create directory");
		}
	}
	else
	{
		clnt->response(530, "Not logged in");
	}
}

void cmd_rmd(ftp_client* clnt, string cmd, string args)
{
	if(isClientAuthorized(clnt))
	{
		string path = getAbsPath(client_cvar[clnt].cwd, args);

		if(sysFsRmdir(path.c_str()) == 0)
		{
			clnt->response(250, "Directory successfully removed");
		}
		else
		{
			clnt->response(550, "Cannot create directory");
		}
	}
	else
	{
		clnt->response(530, "Not logged in");
	}
}

void cmd_cdup(ftp_client* clnt, string cmd, string args)
{
	if(isClientAuthorized(clnt))
	{
		size_t found = client_cvar[clnt].cwd.find_last_of('/');

		if(found == 0)
		{
			found = 1;
		}

		client_cvar[clnt].cwd = client_cvar[clnt].cwd.substr(0, found);

		clnt->response(250, "Directory change successful");
	}
	else
	{
		clnt->response(530, "Not logged in");
	}
}

void cmd_pasv(ftp_client* clnt, string cmd, string args)
{
	if(isClientAuthorized(clnt))
	{
		closesocket(clnt->sock_data);
		closesocket(clnt->sock_pasv);

		clnt->sock_data = -1;

		sockaddr_in sa;
		socklen_t len = sizeof(sa);

		getsockname(clnt->sock_control, (sockaddr *)&sa, &len);

		sa.sin_port = 0; // automatically choose listen port

		clnt->sock_pasv = socket(PF_INET, SOCK_STREAM, 0);

		if(bind(clnt->sock_pasv, (sockaddr*)&sa, sizeof(sa)) == -1)
		{
			closesocket(clnt->sock_pasv);
			clnt->sock_pasv = -1;

			clnt->response(425, "Failed to enter passive mode");
		}

		listen(clnt->sock_pasv, 1);

		// reset rest value
		client_cvar[clnt].rest = 0;

		getsockname(clnt->sock_pasv, (sockaddr*)&sa, &len);

		ostringstream out;
		out << "Entering Passive Mode (";
		out << ((htonl(sa.sin_addr.s_addr) & 0xff000000) >> 24) << ',';
		out << ((htonl(sa.sin_addr.s_addr) & 0x00ff0000) >> 16) << ',';
		out << ((htonl(sa.sin_addr.s_addr) & 0x0000ff00) >>  8) << ',';
		out << (htonl(sa.sin_addr.s_addr) & 0x000000ff) << ',';
		out << ((htons(sa.sin_port) & 0xff00) >> 8) << ',';
		out << (htons(sa.sin_port) & 0x00ff) << ')';

		clnt->response(227, out.str());
	}
	else
	{
		clnt->response(530, "Not logged in");
	}
}

void cmd_port(ftp_client* clnt, string cmd, string args)
{
	if(isClientAuthorized(clnt))
	{
		if(!args.empty())
		{
			clnt->data_close();

			vector<unsigned short> portargs;
			portargs = parsePORT(args);

			if(portargs.size() == 6)
			{
				sockaddr_in sa;
				sa.sin_family = AF_INET;
				sa.sin_port = htons(portargs[4] << 8 | portargs[5]);
				sa.sin_addr.s_addr = htonl(
					((unsigned char)(portargs[0]) << 24) +
					((unsigned char)(portargs[1]) << 16) +
					((unsigned char)(portargs[2]) << 8) +
					((unsigned char)(portargs[3])));

				clnt->sock_data = socket(PF_INET, SOCK_STREAM, 0);

				if(connect(clnt->sock_data, (sockaddr*)&sa, sizeof(sa)) == -1)
				{
					closesocket(clnt->sock_data);
					clnt->sock_data = -1;

					clnt->response(425, "Failed PORT connection");
				}
				else
				{
					// reset rest value
					client_cvar[clnt].rest = 0;

					clnt->response(200, "PORT command successful");
				}
			}
			else
			{
				clnt->response(501, "Invalid PORT connection information");
			}
		}
		else
		{
			clnt->response(501, "No PORT connection information");
		}
	}
	else
	{
		clnt->response(530, "Not logged in");
	}
}

void cmd_abor(ftp_client* clnt, string cmd, string args)
{
	if(isClientAuthorized(clnt))
	{
		if(clnt->sock_data != -1)
		{
			data_client.erase(clnt->sock_data);
		}

		clnt->data_close();
		clnt->response(225, "ABOR command successful");
	}
	else
	{
		clnt->response(530, "Not logged in");
	}
}

void cmd_list(ftp_client* clnt, string cmd, string args)
{
	if(isClientAuthorized(clnt))
	{
		// attempt to open cwd
		if(sysFsOpendir(client_cvar[clnt].cwd.c_str(), &client_cvar[clnt].fd) == 0)
		{
			// open data connection
			if(clnt->data_open(data_list, FTP_DATA_EVENT_SEND))
			{
				// register data handler and set type dvar
				client_cvar[clnt].type = DATA_TYPE_LIST;
				client_cvar[clnt].buffer = new char[DATA_BUFFER];
				data_client[clnt->sock_data] = clnt;
				clnt->response(150, "Accepted data connection");
			}
			else
			{
				sysFsClosedir(client_cvar[clnt].fd);
				clnt->response(425, "Cannot open data connection");
			}
		}
		else
		{
			// cannot open
			clnt->response(550, "Cannot access directory");
		}
	}
	else
	{
		clnt->response(530, "Not logged in");
	}
}

void cmd_mlsd(ftp_client* clnt, string cmd, string args)
{
	if(isClientAuthorized(clnt))
	{
		// attempt to open cwd
		if(sysFsOpendir(client_cvar[clnt].cwd.c_str(), &client_cvar[clnt].fd) == 0)
		{
			// open data connection
			if(clnt->data_open(data_mlsd, FTP_DATA_EVENT_SEND))
			{
				// register data handler and set type dvar
				client_cvar[clnt].type = DATA_TYPE_MLSD;
				client_cvar[clnt].buffer = new char[DATA_BUFFER];
				data_client[clnt->sock_data] = clnt;
				clnt->response(150, "Accepted data connection");
			}
			else
			{
				sysFsClosedir(client_cvar[clnt].fd);
				clnt->response(425, "Cannot open data connection");
			}
		}
		else
		{
			// cannot open
			clnt->response(550, "Cannot access directory");
		}
	}
	else
	{
		clnt->response(530, "Not logged in");
	}
}

void cmd_nlst(ftp_client* clnt, string cmd, string args)
{
	if(isClientAuthorized(clnt))
	{
		// attempt to open cwd
		if(sysFsOpendir(client_cvar[clnt].cwd.c_str(), &client_cvar[clnt].fd) == 0)
		{
			// open data connection
			if(clnt->data_open(data_nlst, FTP_DATA_EVENT_SEND))
			{
				// register data handler and set type dvar
				client_cvar[clnt].type = DATA_TYPE_NLST;
				client_cvar[clnt].buffer = new char[DATA_BUFFER];
				data_client[clnt->sock_data] = clnt;
				clnt->response(150, "Accepted data connection");
			}
			else
			{
				sysFsClosedir(client_cvar[clnt].fd);
				clnt->response(425, "Cannot open data connection");
			}
		}
		else
		{
			// cannot open
			clnt->response(550, "Cannot access directory");
		}
	}
	else
	{
		clnt->response(530, "Not logged in");
	}
}

void cmd_stor(ftp_client* clnt, string cmd, string args)
{
	if(isClientAuthorized(clnt))
	{
		if(!args.empty())
		{
			string path = getAbsPath(client_cvar[clnt].cwd, args);
			s32 oflags = (SYS_O_WRONLY | SYS_O_CREAT);

			// extra flag for append, set rest to 0
			if(cmd == "APPE")
			{
				client_cvar[clnt].rest = 0;
				oflags |= SYS_O_APPEND;
			}

			// extra flag for stor, if rest == 0
			if(client_cvar[clnt].rest == 0)
			{
				oflags |= SYS_O_TRUNC;
			}

			// attempt to open file
			if(sysFsOpen(path.c_str(), oflags, &client_cvar[clnt].fd, NULL, 0) == 0)
			{
				// open data connection
				if(clnt->data_open(data_stor, FTP_DATA_EVENT_RECV))
				{
					// register data handler and set type dvar
					client_cvar[clnt].type = DATA_TYPE_STOR;
					client_cvar[clnt].buffer = new char[DATA_BUFFER];
					data_client[clnt->sock_data] = clnt;
					clnt->response(150, "Accepted data connection");
				}
				else
				{
					sysFsClose(client_cvar[clnt].fd);
					clnt->response(425, "Cannot open data connection");
				}
			}
			else
			{
				// cannot open
				clnt->response(550, "Cannot access directory");
			}
		}
		else
		{
			clnt->response(501, "No filename specified");
		}
	}
	else
	{
		clnt->response(530, "Not logged in");
	}
}

void cmd_retr(ftp_client* clnt, string cmd, string args)
{
	if(isClientAuthorized(clnt))
	{
		if(!args.empty())
		{
			string path = getAbsPath(client_cvar[clnt].cwd, args);
			s32 oflags = SYS_O_RDONLY;

			// attempt to open file
			if(sysFsOpen(path.c_str(), oflags, &client_cvar[clnt].fd, NULL, 0) == 0)
			{
				// open data connection
				if(clnt->data_open(data_retr, FTP_DATA_EVENT_RECV))
				{
					// register data handler and set type dvar
					client_cvar[clnt].type = DATA_TYPE_RETR;
					client_cvar[clnt].buffer = new char[DATA_BUFFER];
					data_client[clnt->sock_data] = clnt;
					clnt->response(150, "Accepted data connection");
				}
				else
				{
					sysFsClose(client_cvar[clnt].fd);
					clnt->response(425, "Cannot open data connection");
				}
			}
			else
			{
				// cannot open
				clnt->response(550, "Cannot access directory");
			}
		}
		else
		{
			clnt->response(501, "No filename specified");
		}
	}
	else
	{
		clnt->response(530, "Not logged in");
	}
}

void cmd_stru(ftp_client* clnt, string cmd, string args)
{
	if(isClientAuthorized(clnt))
	{
		if(args == "F")
		{
			clnt->response(200, "STRU command successful");
		}
		else
		{
			clnt->response(504, "STRU command failed");
		}
	}
	else
	{
		clnt->response(530, "Not logged in");
	}
}

void cmd_mode(ftp_client* clnt, string cmd, string args)
{
	if(isClientAuthorized(clnt))
	{
		if(args == "S")
		{
			clnt->response(200, "MODE command successful");
		}
		else
		{
			clnt->response(504, "MODE command failed");
		}
	}
	else
	{
		clnt->response(530, "Not logged in");
	}
}

void cmd_rest(ftp_client* clnt, string cmd, string args)
{
	if(isClientAuthorized(clnt))
	{
		// C++11
		client_cvar[clnt].rest = strtoull(args.c_str(), NULL, 10);

		if(client_cvar[clnt].rest >= 0)
		{
			ostringstream out;
			out << "Restarting at " << client_cvar[clnt].rest;

			clnt->response(350, out.str());
		}
		else
		{
			client_cvar[clnt].rest = 0;
			clnt->response(554, "Invalid restart point");
		}
	}
	else
	{
		clnt->response(530, "Not logged in");
	}
}

void cmd_dele(ftp_client* clnt, string cmd, string args)
{
	if(isClientAuthorized(clnt))
	{
		if(!args.empty())
		{
			string path = getAbsPath(client_cvar[clnt].cwd, args);

			if(sysFsUnlink(path.c_str()) == 0)
			{
				clnt->response(250, "File successfully removed");
			}
			else
			{
				clnt->response(550, "Cannot remove file");
			}
		}
		else
		{
			clnt->response(501, "No filename specified");
		}
	}
	else
	{
		clnt->response(530, "Not logged in");
	}
}

void cmd_rnfr(ftp_client* clnt, string cmd, string args)
{
	if(isClientAuthorized(clnt))
	{
		if(!args.empty())
		{
			string path = getAbsPath(client_cvar[clnt].cwd, args);

			if(fileExists(path))
			{
				client_cvar[clnt].rnfr = path;
				client_cvar[clnt].cmd = "RNFR";
				clnt->response(350, "RNFR accepted - ready for destination");
			}
			else
			{
				clnt->response(550, "RNFR failed - file does not exist");
			}
		}
		else
		{
			clnt->response(501, "No filename specified");
		}
	}
	else
	{
		clnt->response(530, "Not logged in");
	}
}

void cmd_rnto(ftp_client* clnt, string cmd, string args)
{
	if(isClientAuthorized(clnt))
	{
		if(client_cvar[clnt].cmd == "RNFR")
		{
			if(!args.empty())
			{
				string path = getAbsPath(client_cvar[clnt].cwd, args);

				if(sysLv2FsRename(client_cvar[clnt].rnfr.c_str(), path.c_str()) == 0)
				{
					clnt->response(250, "File successfully renamed");
				}
				else
				{
					clnt->response(550, "Cannot rename file");
				}
			}
			else
			{
				clnt->response(501, "No filename specified");
			}
		}
		else
		{
			clnt->response(503, "Bad command sequence");
		}
	}
	else
	{
		clnt->response(530, "Not logged in");
	}
}

void cmd_site(ftp_client* clnt, string cmd, string args)
{
	if(isClientAuthorized(clnt))
	{
		if(!args.empty())
		{
			string ftpstr(args);

			string::size_type pos = ftpstr.find(' ', 0);
			string sitecmd = ftpstr.substr(0, pos);

			transform(sitecmd.begin(), sitecmd.end(), sitecmd.begin(), ::toupper);

			if(sitecmd == "CHMOD")
			{
				if(pos != string::npos)
				{
					ftpstr = ftpstr.substr(pos + 1);

					pos = ftpstr.find(' ', 0);
					string chmod = ftpstr.substr(0, pos);

					if(pos != string::npos)
					{
						args = ftpstr.substr(pos + 1);

						string path = getAbsPath(client_cvar[clnt].cwd, args);

						if(sysFsChmod(path.c_str(), atoi(chmod.c_str())) == 0)
						{
							clnt->response(250, "Successfully set file permissions");
						}
						else
						{
							clnt->response(550, "Cannot set file permissions");
						}
					}
					else
					{
						clnt->response(501, "Bad command syntax");
					}
				}
				else
				{
					clnt->response(501, "Bad command syntax");
				}
			}
			else
			{
				clnt->response(502, "SITE command not implemented");
			}
		}
		else
		{
			clnt->response(214, "Available SITE commands: CHMOD");
		}
	}
	else
	{
		clnt->response(530, "Not logged in");
	}
}

void cmd_size(ftp_client* clnt, string cmd, string args)
{
	if(isClientAuthorized(clnt))
	{
		if(!args.empty())
		{
			string path = getAbsPath(client_cvar[clnt].cwd, args);

			sysFSStat stat;
			if(sysFsStat(path.c_str(), &stat) == 0)
			{
				ostringstream out;
				out << stat.st_size;

				clnt->response(213, out.str());
			}
			else
			{
				clnt->response(550, "Cannot access file");
			}
		}
		else
		{
			clnt->response(501, "No filename specified");
		}
	}
	else
	{
		clnt->response(530, "Not logged in");
	}
}

void cmd_mdtm(ftp_client* clnt, string cmd, string args)
{
	if(isClientAuthorized(clnt))
	{
		if(!args.empty())
		{
			string path = getAbsPath(client_cvar[clnt].cwd, args);

			sysFSStat stat;
			if(sysFsStat(path.c_str(), &stat) == 0)
			{
				char tstr[15];
				strftime(tstr, 14, "%Y%m%d%H%M%S", localtime(&stat.st_mtime));

				string s(tstr);

				clnt->response(213, s);
			}
			else
			{
				clnt->response(550, "Cannot access file");
			}
		}
		else
		{
			clnt->response(501, "No filename specified");
		}
	}
	else
	{
		clnt->response(530, "Not logged in");
	}
}

void register_cmd(string cmd, cmdhnd handler);

void register_cmds()
{
	// No authorization required commands
	register_cmd("NOOP", cmd_success);
	register_cmd("CLNT", cmd_success);
	register_cmd("ACCT", cmd_ignored);
	register_cmd("QUIT", cmd_quit);
	register_cmd("FEAT", cmd_feat);
	register_cmd("SYST", cmd_syst);
	register_cmd("USER", cmd_user);
	register_cmd("PASS", cmd_pass);

	// Authorization required commands
	register_cmd("TYPE", cmd_success_auth);
	register_cmd("ALLO", cmd_ignored_auth);
	register_cmd("CWD", cmd_cwd);
	register_cmd("PWD", cmd_pwd);
	register_cmd("MKD", cmd_mkd);
	register_cmd("RMD", cmd_rmd);
	register_cmd("CDUP", cmd_cdup);
	register_cmd("PASV", cmd_pasv);
	register_cmd("PORT", cmd_port);
	register_cmd("ABOR", cmd_abor);
	register_cmd("LIST", cmd_list);
	register_cmd("MLSD", cmd_mlsd);
	register_cmd("NLST", cmd_nlst);
	register_cmd("STOR", cmd_stor);
	register_cmd("APPE", cmd_stor);
	register_cmd("RETR", cmd_retr);
	register_cmd("STRU", cmd_stru);
	register_cmd("MODE", cmd_mode);
	register_cmd("REST", cmd_rest);
	register_cmd("DELE", cmd_dele);
	register_cmd("RNFR", cmd_rnfr);
	register_cmd("RNTO", cmd_rnto);
	register_cmd("SITE", cmd_site);
	register_cmd("SIZE", cmd_size);
	register_cmd("MDTM", cmd_mdtm);
}

void event_client_drop(ftp_client* clnt)
{
	if(client_cvar[clnt].type & (DATA_TYPE_LIST | DATA_TYPE_MLSD | DATA_TYPE_NLST))
	{
		// close directory fd
		sysFsClosedir(client_cvar[clnt].fd);
	}
	else
	{
		// close file fd
		sysFsClose(client_cvar[clnt].fd);
	}

	// free memory
	delete [] client_cvar[clnt].buffer;
	client_cvar.erase(clnt);

	if(clnt->sock_data != -1)
	{
		data_client.erase(clnt->sock_data);
	}
}