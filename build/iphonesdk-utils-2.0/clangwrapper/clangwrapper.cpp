#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>

#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <libxml/parser.h>
#include <libxml/tree.h>


#include "helper.h"

using namespace std;

//default arch to armv7
string default_arch = "armv7";
//config file location

extern char ** environ;


int main(int argc, char **argv)
{
  string config_file = string(getenv("HOME")) + "/.iphonesdk";

  // (1) detect toolchain.
  string clang = find_command("clang","clang","clang");
  string ldid = find_command("ldid","ldid","ldid");
  string as = find_command("arm-apple-darwin9-as","arm-apple","as");
  string target = as.substr(as.find("arm"));
  //porting to msys2
#if defined(__CYGWIN__) || defined(__CYGWIN64__)
  if (target.length()>4 && strcasecmp(target.c_str()+target.length()-4, ".exe")==0)
  {
	  target.resize(target.length()-4);
  }
#endif
  target.resize(target.length()-3);

  if(clang.empty() || ldid.empty() || as.empty()) {
    cout <<"Can not find proper toolchain commands."<<endl;
    cout <<"You may need install clang, ldid, cctools" <<endl;
    cout <<"And setup PATH environment variable according to your installation" <<endl;
    exit(0);
  }


  string sdk_fullpath = "/usr/share/iPhoneOS5.0.sdk";
  string version = "5.0";

  int should_init = 0;
  if(endWith(string(argv[0]),"wrapper"))
    should_init = 1;

  if(endWith(string(argv[0]),"ios-switchsdk"))
    should_init = 1;
  //if ~/.iphonesdk not exists, detect it.
  if(::access(config_file.c_str(),R_OK) != 0 || should_init)
    detect_sdk_and_write_configfile(config_file);

  sdk_fullpath = read_sdkpath_from_configfile(config_file);
  version = read_sdkversion_from_configfile(config_file);

  //check sdk path.
  if(::access(sdk_fullpath.c_str(), R_OK) != 0) {
    detect_sdk_and_write_configfile(config_file);
    sdk_fullpath = read_sdkpath_from_configfile(config_file);
    version = read_sdkversion_from_configfile(config_file);
  }

  vector<const char*> additional_args;

  // for SDK 4.x, set default arch to armv6
  if(beginWith(version,"4."))
       default_arch = "armv6";

  string command = "clang";
  string caller = argv[0];
  if(endWith(caller,"ios-clang"))
    command = clang;
  else if(endWith(caller,"ios-clang++"))
  {
#if defined(__CYGWIN__) || defined(__CYGWIN64__)
	  command = clang.substr(0, clang.length()-4);
	  command += "++.exe";
#else
    command = clang + "++";
#endif
	for (int i=1; i<argc; ++i)
	{
		if (strncmp(argv[i], "-stdlib", 7)==0)
			goto skipadd;
	}
	additional_args.push_back("-stdlib=libstdc++");
skipadd:;
  }

  //look in argv, if -arch had been setted, just use the settings.
  for(int i = 0; i < argc; i++) {
    if(strcmp(argv[i],"-arch") == 0 && (i+1) < argc) {
        string arch = argv[i+1];
        if(beginWith(arch,"arm"))
            default_arch = arch;
        break;
    }
  }

  // cmd args for execvpe;
  char **cmd = (char **)malloc((8 + argc + additional_args.size() + 10/*10 is a big enough value for avoiding memory overflow.*/)*sizeof(char*));
  int pos = 0;
  cmd[pos++] = strdup(command.c_str());
  cmd[pos++] = strdup("-target");
  cmd[pos++] = strdup(target.c_str());
  cmd[pos++] = strdup("-arch");
  cmd[pos++] = strdup(default_arch.c_str());
  cmd[pos++] = strdup("-isysroot");
  cmd[pos++] = strdup(sdk_fullpath.c_str());
  cmd[pos++] = strdup("-mlinker-version=134.9");
  for (size_t n=0; n<additional_args.size(); ++n)
	  cmd[pos++] = strdup(additional_args[n]);
  for (int i = 1; i < argc; i ++) {
    cmd[pos++] = argv[i];
  }
  cmd[pos] = (char *)0;

  // env for execvpe
  int count = 0;
  for(int i = 0; environ[i] != NULL; i++ ) {
    count++;
  }

  char ** env_l = (char **)malloc((count+2) * sizeof(char*));
  for(int i = 0; environ[i] != NULL; i++ ) {
    env_l[i] = environ[i];
  }
  string iphone_env = "IPHONEOS_DEPLOYMENT_TARGET=" + version;
  string sign_code_when_build = "IOS_SIGN_CODE_WHEN_BUILD=1";
  env_l[count] = (strdup(iphone_env.c_str()));
  env_l[count+1] = (strdup(sign_code_when_build.c_str()));
  env_l[count+2] = (char *)0;

  //run command.
  execvpe(command.c_str(), cmd, env_l);
  return 0;
}
