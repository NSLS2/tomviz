/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PipelineWorker.h"
#include "Operator.h"

#include <QObject>
#include <QQueue>
#include <QRunnable>
#include <QThreadPool>
#include <QTimer>

#include <vtkDataObject.h>

namespace tomviz {

class PipelineWorker::RunnableOperator : public QObject, public QRunnable
{
  Q_OBJECT

public:
  RunnableOperator(Operator* op, vtkDataObject* input,
                   QObject* parent = nullptr);

  /// Returns the data the operator operates on
  vtkDataObject* data() { return m_data; }
  Operator* op() { return m_operator; }
  void run() override;
  void cancel();
  bool isCanceled();

signals:
  void complete(TransformResult result);

private:
  Operator* m_operator;
  vtkDataObject* m_data;
  Q_DISABLE_COPY(RunnableOperator)
};

class PipelineWorker::Run : public QObject
{
  Q_OBJECT

  enum class State
  {
    CREATED,
    RUNNING,
    CANCELED,
    COMPLETE
  };

public:
  Run(vtkDataObject* data, QList<Operator*> operators);

  /// Clear all Operators from the queue and attempts to cancel the
  /// running Operator.
  void cancel();
  /// Returns true if the operator was successfully removed from the queue
  /// before it was run, false otherwise.
  bool cancel(Operator* op);
  /// Returns true if we are currently running the operator pipeline, false
  /// otherwise.
  bool isRunning();

  /// If the execution of the pipeline is still in progress then add this
  /// operator to it. Return true is
  bool addOperator(Operator* op);

  /// Returns the data object being used for this run.
  vtkDataObject* data() { return m_data; }

  /// Start the pipeline execution
  Future* start();

  QList<Operator*> operators();

public slots:
  void operatorComplete(TransformResult result);

  // Start the next operator in the queue
  void startNextOperator();

signals:
  void finished(bool result);
  void canceled();

private:
  RunnableOperator* m_running = nullptr;
  vtkSmartPointer<vtkDataObject> m_data;
  QQueue<RunnableOperator*> m_runnableOperators;
  QList<RunnableOperator*> m_complete;
  QList<Operator*> m_operators;
  State m_state = State::CREATED;
};
} // namespace tomviz

#include "PipelineWorker.moc"


namespace tomviz {
PipelineWorker::RunnableOperator::RunnableOperator(Operator* op,
                                                   vtkDataObject* data,
                                                   QObject* parent)
  : QObject(parent), m_operator(op), m_data(data)
{
  setAutoDelete(false);
}

void PipelineWorker::RunnableOperator::run()
{
  TransformResult result = m_operator->transform(m_data);
  emit complete(result);
}

void PipelineWorker::RunnableOperator::cancel()
{
  m_operator->cancelTransform();
}

bool PipelineWorker::RunnableOperator::isCanceled()
{
  return m_operator->isCanceled();
}

PipelineWorker::ConfigureThreadPool::ConfigureThreadPool()
{
  auto threads = QThread::idealThreadCount();
  if (threads < 1) {
    threads = 1;
  } else {
    // Use half the threads we have available.
    threads = threads / 2;
  }
  QThreadPool::globalInstance()->setMaxThreadCount(threads);
}

PipelineWorker::Run::Run(vtkDataObject* data, QList<Operator*> operators)
  : m_data(data)
{
  m_operators = operators;
  foreach (auto op, operators) {
    m_runnableOperators.enqueue(new RunnableOperator(op, m_data, this));
  }
}

PipelineWorker::Future* PipelineWorker::Run::start()
{
  auto future = new PipelineWorker::Future(this);
  connect(this, SIGNAL(finished(bool)), future, SIGNAL(finished(bool)));
  connect(this, SIGNAL(canceled()), future, SIGNAL(canceled()));

  QTimer::singleShot(0, this, SLOT(startNextOperator()));

  m_state = State::RUNNING;

  return future;
}

void PipelineWorker::Run::startNextOperator()
{

  if (!m_runnableOperators.isEmpty()) {
    m_running = m_runnableOperators.dequeue();
    connect(m_running, &RunnableOperator::complete, this,
            &PipelineWorker::Run::operatorComplete);
    QThreadPool::globalInstance()->start(m_running);
  }
}

void PipelineWorker::Run::operatorComplete(TransformResult transformResult)
{
  auto runnableOperator = qobject_cast<RunnableOperator*>(sender());

  m_complete.append(runnableOperator);

  bool result = transformResult == TransformResult::Complete;
  // Canceled
  if (m_state == State::CANCELED || runnableOperator->isCanceled()) {
    emit canceled();
  }
  // Error
  else if (!result) {
    emit finished(result);
    // The operator's state shows if it failed.  This complete means the
    // pipeline is no longer running.
    m_state = State::COMPLETE;
  }
  // Run next operator
  else if (!m_runnableOperators.isEmpty()) {
    startNextOperator();
  }
  // We are done
  else {
    m_state = State::COMPLETE;
    emit finished(result);
  }

  runnableOperator->deleteLater();
}

void PipelineWorker::Run::cancel()
{
  m_state = State::CANCELED;
  // Try to cancel the currently running operator
  if (m_running != nullptr) {
    if (QThreadPool::globalInstance()->tryTake(m_running))
      m_running->deleteLater();
    m_running->cancel();
    m_running = nullptr;
  } else {
    emit canceled();
  }
}

bool PipelineWorker::Run::cancel(Operator* op)
{

  // If the operator is currently running we just have to cancel the execution
  // of the whole pipeline.
  if (m_running->op() == op) {
    cancel();
    return false;
  }

  foreach (auto runnable, m_runnableOperators) {
    if (runnable->op() == op) {
      m_runnableOperators.removeAll(runnable);
      return true;
    }
  }

  return false;
}

bool PipelineWorker::Run::isRunning()
{
  return m_state == State::RUNNING;
}

bool PipelineWorker::Run::addOperator(Operator* op)
{
  if (!isRunning()) {
    return false;
  }

  m_runnableOperators.enqueue(new RunnableOperator(op, m_data, this));

  return true;
}

QList<Operator*> PipelineWorker::Run::operators()
{
  return m_operators;
}

PipelineWorker::Future* PipelineWorker::run(vtkDataObject* data, Operator* op)
{
  QList<Operator*> ops;
  ops << op;

  return run(data, ops);
}

PipelineWorker::Future* PipelineWorker::run(vtkDataObject* data,
                                            QList<Operator*> operators)
{
  // Set all the operators in the queued state
  foreach (Operator* op, operators) {
    op->resetState();
  }

  Run* run = new Run(data, operators);

  return run->start();
}

PipelineWorker::Future::Future(Run* run, QObject* parent)
  : QObject(parent), m_run(run)
{}

PipelineWorker::Future::~Future()
{
  m_run->deleteLater();
}

void PipelineWorker::Future::cancel()
{
  m_run->cancel();
}

bool PipelineWorker::Future::cancel(Operator* op)
{
  return m_run->cancel(op);
}

bool PipelineWorker::Future::isRunning()
{
  return m_run->isRunning();
}

vtkDataObject* PipelineWorker::Future::result()
{
  return m_run->data();
}

bool PipelineWorker::Future::addOperator(Operator* op)
{
  return m_run->addOperator(op);
}

QList<Operator*> PipelineWorker::Future::operators()
{
  return m_run->operators();
}

PipelineWorker::PipelineWorker(QObject* parent) : QObject(parent) {}
} // namespace tomviz
