/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "JsonRpcClient.h"

#include <QJsonDocument>
#include <QNetworkAccessManager>

namespace tomviz {

JsonRpcClient::JsonRpcClient(const QString& url, QObject* parent_)
  : QObject(parent_), m_url(url),
    m_networkAccessManager(new QNetworkAccessManager(this))
{}

JsonRpcReply* JsonRpcClient::sendRequest(const QJsonObject& requestBody)
{
  QJsonObject request = requestBody;
  request["jsonrpc"] = QLatin1String("2.0");
  request["id"] = static_cast<int>(m_requestCounter++);

  QNetworkRequest networkRequest(m_url);
  QJsonDocument requestDoc(request);
  QByteArray rpcRequest = requestDoc.toJson(QJsonDocument::Compact);

  networkRequest.setRawHeader("Content-Type", "application/json");
  networkRequest.setRawHeader("Content-Length",
                              QByteArray::number(rpcRequest.size()));

  auto networkReply = m_networkAccessManager->post(networkRequest, rpcRequest);

  auto rpcReply = new JsonRpcReply(this);
  connect(networkReply, &QNetworkReply::finished, [rpcReply, networkReply]() {
    if (networkReply->error() != QNetworkReply::NoError) {
      // The error case is handle by the error signal connection.
      return;
    }
    auto response = networkReply->readAll();
    QJsonParseError errorHandler;
    auto doc = QJsonDocument::fromJson(response, &errorHandler);
    if (errorHandler.error != QJsonParseError::NoError) {
      emit rpcReply->parseError(errorHandler.error, errorHandler.errorString());
      return;
    }

    if (!doc.isObject()) {
      emit rpcReply->protocolError(
        "Response did not contain a valid JSON object.");
      return;
    }

    auto root = doc.object();
    if (root["method"] != QJsonValue::Null) {
      if (root["id"] != QJsonValue::Null) {
        emit rpcReply->protocolError("Received a request for the client.");
      }
    }
    if (root["result"] != QJsonValue::Undefined) {
      emit rpcReply->resultReceived(root);
    } else if (root["error"] != QJsonValue::Null) {
      emit rpcReply->errorReceived(root);
    }
    networkReply->deleteLater();
  });

  connect(networkReply,
          static_cast<void (QNetworkReply::*)(QNetworkReply::NetworkError)>(
            &QNetworkReply::errorOccurred),
          [rpcReply, networkReply](QNetworkReply::NetworkError code) {
            Q_UNUSED(code);
            QVariant statusCode =
              networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute);

            // HTTP error
            if (statusCode.isValid()) {
              auto response = networkReply->readAll();
              QJsonParseError errorHandler;
              auto doc = QJsonDocument::fromJson(response, &errorHandler);

              // Do we have a JSON RPC error message
              if (doc.isObject() && doc.object().contains("error")) {
                emit rpcReply->errorReceived(doc.object()["error"].toObject());
              } else {
                // TODO should probably include the response body
                emit rpcReply->httpError(statusCode.toInt(),
                                         networkReply->errorString());
              }
            } else {
              emit rpcReply->networkError(networkReply->error(),
                                          networkReply->errorString());
            }
            networkReply->deleteLater();
          });

  return rpcReply;
}
} // namespace tomviz
