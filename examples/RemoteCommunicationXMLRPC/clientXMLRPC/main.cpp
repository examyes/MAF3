/*
 *  main.cpp
 *  clientXMLRPC
 *
 *  Created by Paolo Quadrani on 17/09/09.
 *  Copyright 2011 B3C. All rights reserved.
 *
 *  See License at: http://tiny.cc/QXJ4D
 *
 */

#include <QtCore/QCoreApplication>
#include <mafCoreSingletons.h>
#include "mafClientXMLRPC.h"

//#define Operation
//#define ProgrammableOperation
//#define ProgrammableOpenClinicaSearch
//#define SimJobQsub
//#define CalcRisk
//#define Log
//#define cha2ans
#define NMSLoads

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);
    QByteArray ba;
    if(argc != 6) {
        ba = mafTr("Usage: \nclientXMLRPC port server_address dbName workflowId serviceName").toAscii();
        qDebug("%s", ba.data());
        exit(0);
    }

    mafClientXMLRPC client;
    QString port(argv[1]);
    QString address(argv[2]);
    client.connectToServer(port.toUInt(), address);

    //event dictionary
    mafEventBus::mafEvent event_dictionary;
    event_dictionary[TOPIC] = "maf.remote.eventBus.communication.send.xmlrpc";
    event_dictionary[TYPE] = mafEventBus::mafEventTypeRemote;

    //remote event parameters
    mafEventBus::mafEventArgumentsList listToSend;
    QVariantList eventParameters;
    QVariantList dataParameters;
    
#ifdef Operation    
    eventParameters.append("maf.local.resources.operation.executeWithParameters");
    listToSend.append(Q_ARG(QVariantList, eventParameters));

    //remote data parameters (need to send for operation < nameOfOperation , List of QVariant arguments >
    dataParameters.append("mafAlgorithmInterventionalSimulation");
    
    QVariantMap operationParameters;
    operationParameters.insert("storageServiceURI", "http://ws.physiomespace.com/WSExecute.cgi");
    operationParameters.insert("myFirstParameter", 2.3);
    operationParameters.insert("mySecondParameter", "ughetto");
    operationParameters.insert("workflowId", "GUQFEFQUYWJGQJWQBHWGJHWBQHBW");
    
    QStringList inputFiles;
    inputFiles << "L1004024_mesh.cdb" << "component_L1004024_mesh.cdb" << "4024_L1.xml";
    operationParameters.insert("inputFileList", inputFiles);
    QStringList outputFiles;
    outputFiles << "out.cdb" << "out.dat"; 
    operationParameters.insert("outputFileList", outputFiles);
#endif

#ifdef ProgrammableOperation
    eventParameters.append("maf.local.resources.operation.executeWithParameters");
    listToSend.append(Q_ARG(QVariantList, eventParameters));

    //remote data parameters (need to send for operation < nameOfOperation , List of QVariant arguments >
    dataParameters.append("mafAlgorithmProgrammable");
//    dataParameters.append("mafAlgorithmCHA2Samp");

    QVariantMap operationParameters;
    operationParameters.insert("storageServiceURI", "http://ws.physiomespace.com/WSExecute.cgi");
    operationParameters.insert("myFirstParameter", 2.3);
    operationParameters.insert("mySecondParameter", "ughetto");
//    operationParameters.insert("workflowId", "GHPHFPAHFPAHSFAHSPFAPHTP");
    operationParameters.insert("workflowId", "AGJHAGHFAHFASHFIAHSDFHIASDAHSDGI");

    QStringList inputFiles;
    inputFiles << "load_cases_CHA.csv";
    operationParameters.insert("inputFileList", inputFiles);
    QStringList outputFiles;
//    outputFiles << "result.txt";
    outputFiles << "load_cases.txt";
    operationParameters.insert("outputFileList", outputFiles);
    QStringList scriptFiles;
//    scriptFiles << "script.py";
    scriptFiles << "fromCHA2samp_v2.m";
    operationParameters.insert("scriptFileList", scriptFiles);
#endif

#ifdef ProgrammableOpenClinicaSearch
    eventParameters.append("maf.local.resources.operation.executeWithParameters");
    listToSend.append(Q_ARG(QVariantList, eventParameters));

    //remote data parameters (need to send for operation < nameOfOperation , List of QVariant arguments >
    //dataParameters.append("mafAlgorithmProgrammable");
    dataParameters.append("mafAlgorithmOpenClinicaSearch");

    QVariantMap operationParameters;
    operationParameters.insert("storageServiceURI", "http://ws.physiomespace.com/WSExecute.cgi");
    operationParameters.insert("myFirstParameter", 2.3);
    operationParameters.insert("mySecondParameter", "ughetto");
    operationParameters.insert("workflowId", "GHPHFPAHFPAHSFAHSPFAPHTP");

    operationParameters.insert("domain", "openclinica");
    operationParameters.insert("operation", "search");
    operationParameters.insert("requestor", "Charite");
    operationParameters.insert("patientId", "171");

    QStringList inputFiles;
    inputFiles << " ";
    operationParameters.insert("inputFileList", inputFiles);
    QStringList outputFiles;
    outputFiles << "resultValue.txt";
    operationParameters.insert("outputFileList", outputFiles);
#endif

#ifdef SimJobQsub
    eventParameters.append("maf.local.resources.operation.executeWithParameters");
    listToSend.append(Q_ARG(QVariantList, eventParameters));

    //remote data parameters (need to send for operation < nameOfOperation , List of QVariant arguments >
    dataParameters.append("mafAlgorithmSimJobQsub");

    QVariantMap operationParameters;
    operationParameters.insert("storageServiceURI", "http://ws.physiomespace.com/WSExecute.cgi");
    operationParameters.insert("myFirstParameter", 2.3);
    operationParameters.insert("mySecondParameter", "ughetto");
    operationParameters.insert("jobsNumber", 20);
    operationParameters.insert("workflowId", "ALEQFHPHAFSPHFAPSHFPHFPSDHF");

    QStringList inputFiles;
//    inputFiles << "load_cases_tot.txt";
//    inputFiles << "load_cases_tot.txt" << "file_pulled_2" << "file_pulled_3" << "file_pulled_4" << "file_pulled_5" << "file_pulled_6" << "file_pulled_7" << "file_pulled_8"  << "file_pulled_9" << "file_year.lis";
    //inputFiles << "load_cases_tot.txt" << "https://www.physiomespace.com/Members/vphop-project/dataresource.2012-07-16.1342441306576" << "https://www.physiomespace.com/Members/vphop-project/dataresource.2012-07-16.1342441274629"  << "https://www.physiomespace.com/Members/vphop-project/dataresource.2012-07-16.1342441286972" <<  "https://www.physiomespace.com/Members/vphop-project/dataresource.2012-07-16.1342441295344"  << "https://www.physiomespace.com/Members/vphop-project/dataresource.2012-07-16.1342441327200"  <<  "https://www.physiomespace.com/Members/vphop-project/dataresource.2012-07-16.1342441332472" << "https://www.physiomespace.com/Members/vphop-project/dataresource.2012-07-16.1342441338460" << "https://www.physiomespace.com/Members/vphop-project/dataresource.2012-09-03.1346702681267";
    inputFiles << "load_cases_tot.txt" << "https://www.physiomespace.com/Members/vphop-project/dataresource.2012-09-03.1346702647587" << "https://www.physiomespace.com/Members/vphop-project/dataresource.2012-09-03.1346702612095"  << "https://www.physiomespace.com/Members/vphop-project/dataresource.2012-09-03.1346702625587" <<  "https://www.physiomespace.com/Members/vphop-project/dataresource.2012-09-03.1346702636999"  << "https://www.physiomespace.com/Members/vphop-project/dataresource.2012-09-03.1346702661463"  <<  "https://www.physiomespace.com/Members/vphop-project/dataresource.2012-09-03.1346702665923" << "https://www.physiomespace.com/Members/vphop-project/dataresource.2012-09-03.1346702673708" << "https://www.physiomespace.com/Members/vphop-project/dataresource.2012-09-03.1346702681267";
    operationParameters.insert("inputFileList", inputFiles);
    QStringList outputFiles;
    outputFiles << "Results_TOT.pdrs";
    operationParameters.insert("outputFileList", outputFiles);
#endif

#ifdef CalcRisk
    eventParameters.append("maf.local.resources.operation.executeWithParameters");
    listToSend.append(Q_ARG(QVariantList, eventParameters));

    //remote data parameters (need to send for operation < nameOfOperation , List of QVariant arguments >
    dataParameters.append("mafAlgorithmCalculateRisk");

    QVariantMap operationParameters;
    operationParameters.insert("storageServiceURI", "http://ws.physiomespace.com/WSExecute.cgi");
    operationParameters.insert("myFirstParameter", 2.3);
    operationParameters.insert("mySecondParameter", "ughetto");
    operationParameters.insert("yearNumber", 4);
    operationParameters.insert("workflowId", "FGHGFNMSIEOAKMVOESLLCVRT");

    QStringList inputFiles;
    //inputFiles << "Results_TOT_con_ciclo.pdrs";
    inputFiles << "https://www.physiomespace.com/Members/vphop-project/dataresource.2012-09-17.1347897249351";
    //inputFiles << "https://www.physiomespace.com/Members/testUser/dataresource.2012-09-17.1347897249351";
    operationParameters.insert("inputFileList", inputFiles);
    QStringList outputFiles;
    outputFiles << "outLines.txt" << "RF.txt";
    operationParameters.insert("outputFileList", outputFiles);
#endif

#ifdef cha2ans
    eventParameters.append("maf.local.resources.operation.executeWithParameters");
    listToSend.append(Q_ARG(QVariantList, eventParameters));

    //remote data parameters (need to send for operation < nameOfOperation , List of QVariant arguments >
    dataParameters.append("mafAlgorithmCHA2Ans");

    QVariantMap operationParameters;
    operationParameters.insert("storageServiceURI", "http://ws.physiomespace.com/WSExecute.cgi");
    operationParameters.insert("myFirstParameter", 2.3);
    operationParameters.insert("mySecondParameter", "ughetto");
    operationParameters.insert("yearNumber", 4);
    operationParameters.insert("workflowId", "MAMMHGHAOIRAIXAFKDOOEJMM");

    QStringList inputFiles;
    inputFiles << "load_cases.txt";
    //inputFiles << "https://www.physiomespace.com/Members/vphop-project/dataresource.2012-09-17.1347897249351";
    //inputFiles << "https://www.physiomespace.com/Members/testUser/dataresource.2012-09-17.1347897249351";
    operationParameters.insert("inputFileList", inputFiles);
    QStringList outputFiles;
    outputFiles << "load_case_tot.txt";
    operationParameters.insert("outputFileList", outputFiles);
#endif

#ifdef NMSLoads
    eventParameters.append("maf.local.resources.operation.executeWithParameters");
    listToSend.append(Q_ARG(QVariantList, eventParameters));

    //remote data parameters (need to send for operation < nameOfOperation , List of QVariant arguments >
    dataParameters.append("mafAlgorithmNMSLoads");

    QVariantMap operationParameters;
    operationParameters.insert("storageServiceURI", "http://ws.physiomespace.com/WSExecute.cgi");
    operationParameters.insert("myFirstParameter", 2.3);
    operationParameters.insert("mySecondParameter", "ughetto");
    operationParameters.insert("workflowId", "OSLOFOGJSONEIFOLEPJJJJJJJJ");

    QStringList inputFiles;
    //inputFiles << "BW.txt" << "Head_Radius.lis" << "side_femur.lis";
    inputFiles << "BW.txt" << "https://www.physiomespace.com/Members/vphop-project/dataresource.2012-09-03.1346702625587" << "https://www.physiomespace.com/Members/vphop-project/dataresource.2012-09-03.1346702661463";
    operationParameters.insert("inputFileList", inputFiles);
    QStringList outputFiles;
    outputFiles << "OUTPUT_FORCES.txt" << "MAT_ROT_ISB_GLOB.txt" << "KP_ISB.txt";
    operationParameters.insert("outputFileList", outputFiles);
#endif

    
#ifdef Log    
    eventParameters.append("maf.local.resources.operation.executeWithParameters");
    listToSend.append(Q_ARG(QVariantList, eventParameters));
    
    //remote data parameters (need to send for operation < nameOfOperation , List of QVariant arguments >
    //dataParameters.append("mafOperationManageLog");
    dataParameters.append("mafOperationLogger");
    
    QVariantMap operationParameters;
    operationParameters.insert("dataBaseName", argv[3]);
    operationParameters.insert("tableName", "logTable");
    operationParameters.insert("workflowId", argv[4]);
    operationParameters.insert("serviceName", argv[5]);
    
#endif
    
    dataParameters.push_back(operationParameters);
    listToSend.append(Q_ARG(QVariantList, dataParameters));

    client.sendRequestToServer(&event_dictionary, &listToSend);

    return a.exec();
}
