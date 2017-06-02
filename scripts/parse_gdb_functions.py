import re
import copy
from collections import defaultdict
"""
Logic to find the function names and their return type
returns a dictionay with function names,ids 

"""
def get_function_sign(filepath = "src/lua_src/full_log.txt"):
	count = 0
	flag=0
	all_functions_with_name=[]
	final=[]

	my_dict=defaultdict(lambda: {})
	param=[]
	fname_flag=0
	func_start=0
	in_param=0
	param_name=0
	ret_flag=-1
	before_param=0
	extern=0
	naut_file=1
	with open(filepath) as elflog:
		for line in elflog:
			if "DW_AT_name" in line and "src/nautilus/" in line:
				naut_file = 1
			elif "DW_AT_name" in line and "src/lua_src/" in line:
				naut_file = 0
			if naut_file==1:
				if "<1>" in line:
					func_start=0
					in_param=0
					param_name=0
				m = re.findall("\s+[<\w+>]*<(\w+)>:*\s+([\w\s]*:)(.*)",line)
				if m and "DW_TAG_subprogram" in m[0][2] and "<1>" in line:
					count+=1
					#if len(my_dict.keys())!=0:
					#	final.append(copy.deepcopy(my_dict))
					#my_dict={}
					func_start=1
					before_param=1
					extern_flag=1
					fname_flag=0
					in_param=0
					param_name=0
					ret_flag=0
					extern=0
					func_addr=m[0][0]
					#print str(in_param)+"---"+str(param_name)
					continue
				if fname_flag==0 and func_start==1 and in_param==0 and param_name==0 and before_param==1:
					#m = re.findall("\s+[<\w+>]*<(\w+)>:*\s+([\w\s]*:)(.*)",line)
					#print line 
					if "DW_AT_name" in line:
						func_name=(line.split(":")[-1]).strip()
						all_functions_with_name.append(func_name)
						fname_flag=1
						my_dict[func_addr]["name"]=func_name
						my_dict[func_addr]["param"]=[]
						if extern==1:
							my_dict[func_addr]["extern"]=1
						#print func_name
						continue
				if func_start==1 and in_param==0 and before_param==1:
					if "DW_AT_inline" in line:
						fname_flag=1
						func_start=0
						if fname_flag==1:
							my_dict.pop(func_addr)
				if func_start==1 and in_param==0 and before_param==1:
					if "DW_AT_external" in line:
						extern=1
				if func_start==1 and in_param==0 and before_param==1:
					if "DW_AT_low_pc" in line:
						if extern==1:
							if ret_flag==0:
								my_dict[func_addr]["ret_type"]= "void"
								ret_flag=1
						else:
							my_dict[func_addr]["static"]=1
							
				if ret_flag==0 and func_start==1 and in_param==0 and param_name==0 and before_param==1:
					#m = re.findall("\s+[<\w+>]*<(\w+)>:*\s+([\w\s]*:)(.*)",line)
					#print line 
					if "DW_AT_type" in line:
						ret=(line.split(":")[-1]).strip()
						ret_flag=1
						my_dict[func_addr]["ret_type"]=ret
						#print func_name
						continue
				if func_start==1 and in_param==0 and param_name==0:
					if "<2>" in line and "DW_TAG_formal_parameter" in line:
						in_param=1
						before_param=0
						if ret_flag==0:
							my_dict[func_addr]["ret_type"]= "void"
							ret_flag=1
						extern_flag=0
						#print line
						continue
					if "<2>" in line and "DW_TAG_unspecified_parameters" in line:
						my_dict[func_addr]["param"].append(["...","..."])
						if ret_flag==0:
							my_dict[func_addr]["ret_type"]= "void"
							ret_flag=1
						extern_flag=0
						before_param=0
				if fname_flag==1 and func_start==1 and in_param==1 and param_name==0:
					if "DW_AT_name" in line:
						param.append(line.split(":")[-1].strip())
						param_name=1
						continue
				if fname_flag==1 and func_start==1 and in_param==1 and param_name==1:
					if "DW_AT_type" in line:
						param.append(line.split(":")[-1].strip())
						#print param
						my_dict[func_addr]["param"].append(param)
						if len(param)!=2:
							print "error_param"
						param=[]
						in_param=0
						param_name=0
						continue
			#final.append(copy.deepcopy(my_dict))
		for k, v in my_dict.items():
			fname= v.get("name","") 
			if fname!="":
				all_functions_with_name.remove(fname)
		return my_dict, all_functions_with_name


#function_values = get_function_sign()


"""
#print "length of final list = "+str(len(function_values.keys()))
for k,v in function_values.items():
	print k+" : "+str(v)

"""
