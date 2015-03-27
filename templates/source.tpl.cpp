// 标准C库文件
#include <stdio.h>
#include <string.h>
#include <cstdlib>

// CTP头文件
#include <ThostFtdcTraderApi.h>
#include <ThostFtdcMdApi.h>
#include <ThostFtdcUserApiDataType.h>
#include <ThostFtdcUserApiStruct.h>

// 线程控制相关
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

// 字符串编码转化
#include <code_convert.h>

// 登录请求结构体
CThostFtdcReqUserLoginField userLoginField;
// 用户请求结构体
CThostFtdcUserLogoutField userLogoutField;
// 线程同步标志
sem_t sem;
// requestID
int requestID = 0;


class CTraderHandler : public CThostFtdcTraderSpi{

	public:

    CTraderHandler(){
        printf("CTraderHandler():被执行...\n");
    }

	// 允许登录事件
    virtual void OnFrontConnected() {
		static int i = 0;
		printf("OnFrontConnected():被执行...\n");
		// 在登出后系统会重新调用OnFrontConnected，这里简单判断并忽略第1次之后的所有调用。
		if (i++==0) {
			sem_post(&sem);
		}
    }

	// 登录结果响应
    virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,
    	CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast){
        printf("OnRspUserLogin():被执行...\n");
		if (pRspInfo->ErrorID == 0) {
			printf("登录成功!\n");
			sem_post(&sem);
		}else{
			printf("登录失败!\n");
		}
    }

	// 登出结果响应
	virtual void OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout,
		CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
		printf("OnReqUserLogout():被执行...\n");
		if (pRspInfo->ErrorID == 0) {
			printf("登出成功!\n");
			sem_post(&sem);
		}else{
			printf("登出失败!\n");
		}
	}

	// 错误信息响应方法
	virtual void OnRspError
		(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast){		
		printf("OnRspError():被执行...\n");
	}

	// 查询合约结果响应
	//virtual void OnRspQryInstrument
	//	(ThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo,int nRequestID, bool bIsLast) {
		//static int i=0;
		//char InstrumentName[100];
		//codeConvert((char *)"GBK",(char*)"UTF8",pInstrument->InstrumentName,InstrumentName,sizeof(InstrumentName));			
		//printf("InstrumentID=%s,ExchangeID=%s,InstrumentName=%s\n",
		//pInstrument->InstrumentID,pInstrument->ExchangeID,InstrumentName);
		//i++;
		//if(bIsLast){
		//	printf("一共有%d合约可供交易\n",i);
		//	sem_post(&sem);
		//}	
	//}

	virtual {{ responseMethod['returns'] }} {{ responseMethod['name'] }}(
		{% for parameter in respParameters -%}
			{{ 	parameter['type'] }} {{ parameter['name'] -}}
			{% if not loop.last	%},{% endif %}
		{% endfor -%}
	){		
		printf("{{ responseMethod['name'] }}():被执行...\n");
		{{respParameters[0]['raw_type']}} responseData;
		// 读取返回信息,并做编码转化
		{% for field in responseFields -%}
			{{field['doxygen'].decode('utf8')}} {{ field['original'] -}} 
			{% if field['len'] %}[{{  field['len'] }}]{% endif %}
			{% if field['original'] == 'char' and field['len'] != None -%}
				strcpy(responseData.{{ field['name'] }},"");
			{% elif field['original'] == 'char' and field['len'] == None -%}
				responseData.{{ field['name'] }} = '0';
			{% elif field['original'] != 'char' and field['len'] == None -%}
				responseData.{{ field['name'] }} = 0;
			{% else -%}
				//responseData.{{ field['name'] }} = ;
			{% endif -%}
		{% endfor %}		
		
		// 如果响应函数已经返回最后一个信息
		if(bIsLast){
			// 通知主过程，响应函数将结束
			sem_post(&sem);
		}	
	}

};


int main(){

	// 初始化线程同步变量
	sem_init(&sem,0,0);

	// 从环境变量中读取登录信息
	char * CTP_FrontAddress = getenv("CTP_FrontAddress");
	if ( CTP_FrontAddress == NULL ){
		printf("环境变量CTP_FrontAddress没有设置\n");
		return(0);
	}
	
	char * CTP_BrokerId = getenv("CTP_BrokerId");
	if ( CTP_BrokerId == NULL ){
		printf("环境变量CTP_BrokerId没有设置\n");
		return(0);
	}
	strcpy(userLoginField.BrokerID,CTP_BrokerId);

	char * CTP_UserId = getenv("CTP_UserId");
	if ( CTP_UserId == NULL ){
		printf("环境变量CTP_UserId没有设置\n");
		return(0);
	}	
	strcpy(userLoginField.UserID,CTP_UserId);

	char * CTP_Password = getenv("CTP_Password");
	if ( CTP_Password == NULL ) {
		printf("环境变量CTP_Password没有设置\n");
		return(0);
	}
	strcpy(userLoginField.Password,CTP_Password);

	// 创建TraderAPI和回调响应控制器的实例
	CThostFtdcTraderApi *pTraderApi = CThostFtdcTraderApi::CreateFtdcTraderApi();
	CTraderHandler traderHandler = CTraderHandler();
	CTraderHandler * pTraderHandler = &traderHandler;
	pTraderApi->RegisterSpi(pTraderHandler);

	// 设置服务器地址
	pTraderApi->RegisterFront(CTP_FrontAddress);
	printf("CTP_FrontAddress=%s",CTP_FrontAddress);
	// 链接交易系统
	pTraderApi->Init();
	// 等待服务器发出登录消息
	sem_wait(&sem);
	// 发出登陆请求
	pTraderApi->ReqUserLogin(&userLoginField, requestID++);
	// 等待登录成功消息
	sem_wait(&sem);

	////////////////////////////////////////////////////////////////////////////////////////////////

	// 定义调用API的数据结构
	{{reqParameters[0]['raw_type']}} requestData;
	// 确保没有初始化的数据不会被访问
	memset(&requestData,0,sizeof(requestData));
	// 为调用结构题设置参数信息
	{% for field in requestFields -%}
		{{field['doxygen'].decode('utf8')}}
		{% if field['original'] == 'char' and field['len'] != None -%}
			strcpy(requestData.{{ field['name'] }},"");
		{% elif field['original'] == 'char' and field['len'] == None -%}
			requestData.{{ field['name'] }} = '0';
		{% elif field['original'] != 'char' and field['len'] == None -%}
			requestData.{{ field['name'] }} = 0;
		{% else -%}
			//requestData.{{ field['name'] }} = ;
		{% endif -%}
	{% endfor %}		

	// 调用API,并等待响应函数返回
	int result = pTraderApi->{{ requestMethod['name'] }}(&requestData,requestID++);
	sem_wait(&sem);	

	/////////////////////////////////////////////////////////////////////////////////////////////////
	

	// 拷贝用户登录信息到登出信息
	strcpy(userLogoutField.BrokerID,userLoginField.BrokerID);
	strcpy(userLogoutField.UserID, userLoginField.UserID);
	pTraderApi->ReqUserLogout(&userLogoutField, requestID++);

	// 等待登出成功
	sem_wait(&sem);

	printf("主线程执行完毕!\n");
	return(0);

}
