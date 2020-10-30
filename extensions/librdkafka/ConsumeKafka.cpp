/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ConsumeKafka.h"

#include <algorithm>

#include "core/PropertyValidation.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property ConsumeKafka::KafkaBrokers(core::PropertyBuilder::createProperty("Kafka Brokers")
  ->withDescription("A comma-separated list of known Kafka Brokers in the format <host>:<port>.")
  ->withDefaultValue("localhost:9092", core::StandardValidators::get().NON_BLANK_VALIDATOR)
  ->supportsExpressionLanguage(true)
  ->isRequired(true)
  ->build());

core::Property ConsumeKafka::SecurityProtocol(core::PropertyBuilder::createProperty("Security Protocol")
  ->withDescription("Protocol used to communicate with brokers. Corresponds to Kafka's 'security.protocol' property.")
  ->withAllowableValues<std::string>({SECURITY_PROTOCOL_PLAINTEXT, SECURITY_PROTOCOL_SSL, SECURITY_PROTOCOL_SASL_PLAINTEXT, SECURITY_PROTOCOL_SASL_SSL})
  ->withDefaultValue(SECURITY_PROTOCOL_PLAINTEXT)
  ->isRequired(true)
  ->build());

core::Property ConsumeKafka::TopicNames(core::PropertyBuilder::createProperty("Topic Names")
  ->withDescription("The name of the Kafka Topic(s) to pull from. More than one can be supplied if comma separated.")
  ->supportsExpressionLanguage(true)
  ->build());

core::Property ConsumeKafka::TopicNameFormat(core::PropertyBuilder::createProperty("Topic Name Format")
  ->withDescription("Specifies whether the Topic(s) provided are a comma separated list of names or a single regular expression.")
  ->withAllowableValues<std::string>({TOPIC_FORMAT_NAMES, TOPIC_FORMAT_PATTERNS})
  ->withDefaultValue(TOPIC_FORMAT_NAMES)
  ->build());

core::Property ConsumeKafka::HonorTransactions(core::PropertyBuilder::createProperty("Honor Transactions")
  ->withDescription(
      "Specifies whether or not NiFi should honor transactional guarantees when communicating with Kafka. If false, the Processor will use an \"isolation level\" of "
      "read_uncomitted. This means that messages will be received as soon as they are written to Kafka but will be pulled, even if the producer cancels the transactions. "
      "If this value is true, NiFi will not receive any messages for which the producer's transaction was canceled, but this can result in some latency since the consumer "
      "must wait for the producer to finish its entire transaction instead of pulling as the messages become available.")
  ->withDefaultValue<bool>(true)
  ->isRequired(true)
  ->build());

core::Property ConsumeKafka::GroupID(core::PropertyBuilder::createProperty("Group ID")
  ->withDescription("A Group ID is used to identify consumers that are within the same consumer group. Corresponds to Kafka's 'group.id' property.")
  ->supportsExpressionLanguage(true)
  ->isRequired(true)
  ->build());

core::Property ConsumeKafka::OffsetReset(core::PropertyBuilder::createProperty("Offset Reset")
  ->withDescription("Allows you to manage the condition when there is no initial offset in Kafka or if the current offset does not exist any more on the server (e.g. because that "
      "data has been deleted). Corresponds to Kafka's 'auto.offset.reset' property.")
  ->withAllowableValues<std::string>({OFFSET_RESET_EARLIEST, OFFSET_RESET_LATEST, OFFSET_RESET_NONE})
  ->withDefaultValue(OFFSET_RESET_LATEST)
  ->isRequired(true)
  ->build());

core::Property ConsumeKafka::KeyAttributeEncoding(core::PropertyBuilder::createProperty("Key Attribute Encoding")
  ->withDescription("FlowFiles that are emitted have an attribute named 'kafka.key'. This property dictates how the value of the attribute should be encoded.")
  ->withAllowableValues<std::string>({KEY_ATTR_ENCODING_UTF_8, KEY_ATTR_ENCODING_HEX})
  ->withDefaultValue(KEY_ATTR_ENCODING_UTF_8)
  ->isRequired(true)
  ->build());

core::Property ConsumeKafka::MessageDemarcator(core::PropertyBuilder::createProperty("Message Demarcator")
  ->withDescription("Since KafkaConsumer receives messages in batches, you have an option to output FlowFiles which contains all Kafka messages in a single batch "
      "for a given topic and partition and this property allows you to provide a string (interpreted as UTF-8) to use for demarcating apart multiple Kafka messages. "
      "This is an optional property and if not provided each Kafka message received will result in a single FlowFile which time it is triggered. "
      "To enter special character such as 'new line' use CTRL+Enter or Shift+Enter depending on the OS.")
  ->supportsExpressionLanguage(true)
  ->build());

core::Property ConsumeKafka::MessageHeaderEncoding(core::PropertyBuilder::createProperty("Message Header Encoding")
  ->withDescription("Any message header that is found on a Kafka message will be added to the outbound FlowFile as an attribute. This property indicates the Character Encoding "
      "to use for deserializing the headers.")
  ->withAllowableValues<std::string>({MSG_HEADER_ENCODING_UTF_8, MSG_HEADER_ENCODING_HEX})
  ->withDefaultValue(MSG_HEADER_ENCODING_UTF_8)
  ->build());

core::Property ConsumeKafka::HeadersToAddAsAttributes(core::PropertyBuilder::createProperty("Headers To Add As Attributes")
  ->withDescription("A Regular Expression that is matched against all message headers. Any message header whose name matches the regex will be added to the FlowFile "
      "as an Attribute. If not specified, no Header values will be added as FlowFile attributes. If two messages have a different value for the same header and that "
      "header is selected by the provided regex, then those two messages must be added to different FlowFiles. As a result, users should be cautious about using a "
      "regex like \".*\" if messages are expected to have header values that are unique per message, such as an identifier or timestamp, because it will prevent MiNiFi "
      "from bundling the messages together efficiently.")
  ->build());

core::Property ConsumeKafka::DuplicateHeaderHandling(core::PropertyBuilder::createProperty("Duplicate Header Handling")
  ->withDescription("For headers to be added as attributes, this option specifies how to handle cases where multiple headers are present with the same key. "
      "For example in case of receiving these two headers: \"Accept: text/html\" and \"Accept: application/xml\" and we want to attach the value of \"Accept\" "
      "as a FlowFile attribute:\n"
      " - \"Keep First\" attaches: \"Accept -> text/html\"\n"
      " - \"Keep Latest\" attaches: \"Accept -> application/xml\"\n"
      " - \"Comma-separated Merge\" attaches: \"Accept -> text/html, application/xml\"\n")
  ->withAllowableValues<std::string>({MSG_HEADER_KEEP_FIRST, MSG_HEADER_KEEP_LATEST, MSG_HEADER_COMMA_SEPARATED_MERGE})
  ->withDefaultValue(MSG_HEADER_KEEP_LATEST) // Mirroring NiFi behaviour
  ->build());

core::Property ConsumeKafka::MaxPollRecords(core::PropertyBuilder::createProperty("Max Poll Records")
  ->withDescription("Specifies the maximum number of records Kafka should return in a single poll.")
  ->withDefaultValue<unsigned int>(10000)
  ->build());

core::Property ConsumeKafka::MaxUncommittedTime(core::PropertyBuilder::createProperty("Max Uncommitted Time")
  ->withDescription("Specifies the maximum amount of time allowed to pass before offsets must be committed. This value impacts how often offsets will be committed. "
      "Committing offsets less often increases throughput but also increases the window of potential data duplication in the event of a rebalance or FlowController restart between commits."
      "This value is also related to maximum poll records and the use of a message demarcator. When using a message demarcator we can have far more uncommitted messages than when we're not "
      "as there is much less for us to keep track of in memory.")
  ->withDefaultValue<core::TimePeriodValue>("1 second")
  ->build());

core::Property ConsumeKafka::CommunicationsTimeout(core::PropertyBuilder::createProperty("Communications Timeout")
  ->withDescription("Specifies the timeout that the consumer should use when communicating with the Kafka Broker")
  ->withDefaultValue<core::TimePeriodValue>("60 seconds")
  ->isRequired(true)
  ->build());

const core::Relationship ConsumeKafka::Success("success", "Incoming kafka messages as flowfiles");

void ConsumeKafka::initialize() {
  setSupportedProperties({
    KafkaBrokers,
    SecurityProtocol,
    TopicNames,
    TopicNameFormat,
    HonorTransactions,
    GroupID,
    OffsetReset,
    KeyAttributeEncoding,
    MessageDemarcator,
    MessageHeaderEncoding,
    HeadersToAddAsAttributes,
    DuplicateHeaderHandling,
    MaxPollRecords,
    MaxUncommittedTime,
    CommunicationsTimeout
  });
  setSupportedRelationships({
    Success,
  });
}

void ConsumeKafka::onSchedule(core::ProcessContext* context, core::ProcessSessionFactory* /* sessionFactory */) {
  // Required properties
  kafka_brokers_                       = utils::listFromRequiredCommaSeparatedProperty(context, KafkaBrokers.getName());
  security_protocol_                   = utils::getRequiredPropertyOrThrow(context, SecurityProtocol.getName());
  topic_names_                         = utils::listFromRequiredCommaSeparatedProperty(context, TopicNames.getName());
  topic_name_format_                   = utils::getRequiredPropertyOrThrow(context, TopicNameFormat.getName());
  honor_transactions_                  = utils::parseBooleanPropertyOrThrow(context, HonorTransactions.getName());
  group_id_                            = utils::getRequiredPropertyOrThrow(context, GroupID.getName());
  offset_reset_                        = utils::getRequiredPropertyOrThrow(context, OffsetReset.getName());
  key_attribute_encoding_              = utils::getRequiredPropertyOrThrow(context, KeyAttributeEncoding.getName());
  communications_timeout_milliseconds_ = utils::parseTimePropertyMSOrThrow(context, CommunicationsTimeout.getName());

  // Optional properties
  context->getProperty(MessageDemarcator.getName(), message_demarcator_);
  context->getProperty(MessageHeaderEncoding.getName(), message_header_encoding_);
  context->getProperty(DuplicateHeaderHandling.getName(), duplicate_header_handling_);

  headers_to_add_as_attributes_ = utils::listFromCommaSeparatedProperty(context, HeadersToAddAsAttributes.getName());
  max_poll_records_ = utils::getOptionalUintProperty(context, MaxPollRecords.getName());
  max_uncommitted_time_seconds_ = utils::getOptionalUintProperty(context, MaxUncommittedTime.getName());
  // TODO(hunyadi): add dynamic property support for kafka configuration properties
  // readDynamicPropertyKeys(context);

  configureNewConnection(context);
}

// TODO(hunyadi): maybe move this to the kafka utils?
void print_topics_list(std::shared_ptr<logging::Logger> logger, rd_kafka_topic_partition_list_t* kf_topic_partition_list) {
  for (std::size_t i = 0; i < kf_topic_partition_list->cnt; ++i) {
    logger->log_debug("kf_topic_partition_list: \u001b[33m[topic: %s, partition: %d, offset:%lld] \u001b[0m",
    kf_topic_partition_list->elems[i].topic, kf_topic_partition_list->elems[i].partition, kf_topic_partition_list->elems[i].offset);
  }
}

// TODO(hunyadi): maybe move this to the kafka utils?


// TODO(hunyadi): this is a test for trying to manually set new connections offsets to latest
void rebalance_cb(rd_kafka_t* rk, rd_kafka_resp_err_t err, rd_kafka_topic_partition_list_t* partitions, void* /*opaque*/) {
  std::shared_ptr<logging::Logger> logger{logging::LoggerFactory<ConsumeKafka>::getLogger()};
  logger->log_debug("\u001b[37;1mRebalance triggered.\u001b[0m");
  switch (err) {
    case RD_KAFKA_RESP_ERR__ASSIGN_PARTITIONS:
      logger->log_debug("assigned");
      // FIXME(hunyadi): this should only happen when running the tests -> move this implementation there
      // for (std::size_t i = 0; i < partitions->cnt; ++i) {
      //   partitions->elems[i].offset = RD_KAFKA_OFFSET_END;
      // }
      print_topics_list(logger, partitions);
      rd_kafka_assign(rk, partitions);
      break;

    case RD_KAFKA_RESP_ERR__REVOKE_PARTITIONS:
      logger->log_debug("revoked:");
      print_topics_list(logger, partitions);
      rd_kafka_assign(rk, NULL);
      break;

    default:
      logger->log_debug("failed: %s", rd_kafka_err2str(err));
      rd_kafka_assign(rk, NULL);
      break;
  }
}

void ConsumeKafka::createTopicPartitionList() {
  kf_topic_partition_list_ = { rd_kafka_topic_partition_list_new(topic_names_.size()), utils::rd_kafka_topic_partition_list_deleter() };

  // On subscriptions any topics prefixed with ^ will be regex matched
  if (topic_name_format_ == "pattern") {
    for (const std::string& topic : topic_names_) {
      const std::string regex_format = "^" + topic;
      rd_kafka_topic_partition_list_add(kf_topic_partition_list_.get(), regex_format.c_str(), RD_KAFKA_PARTITION_UA);  // TODO(hunyadi): check RD_KAFKA_PARTITION_UA
    }
  } else {
    for (const std::string& topic : topic_names_) {
      rd_kafka_topic_partition_list_add(kf_topic_partition_list_.get(), topic.c_str(), RD_KAFKA_PARTITION_UA);  // TODO(hunyadi): check RD_KAFKA_PARTITION_UA
    }
  }

  // Subscribe to topic set using balanced consumer groups
  // Subscribing from the same process without an inbetween unsubscribe call
  // Does not seem to be triggering a rebalance (maybe librdkafka bug?)
  // This might happen until the cross-overship between processors and connections are settled
  rd_kafka_resp_err_t subscribe_response = rd_kafka_subscribe(consumer_.get(), kf_topic_partition_list_.get());
  if (RD_KAFKA_RESP_ERR_NO_ERROR != subscribe_response) {
    logger_->log_error("\u001b[31mrd_kafka_subscribe error %d: %s\u001b[0m", subscribe_response, rd_kafka_err2str(subscribe_response));
  }
}

void ConsumeKafka::configureNewConnection(const core::ProcessContext* context) {
  using utils::setKafkaConfigurationField;

  conf_ = { rd_kafka_conf_new(), utils::rd_kafka_conf_deleter() };
  if (conf_ == nullptr) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Failed to create rd_kafka_conf_t object");
  }

  // Set rebalance callback for use with coordinated consumer group balancing
  // Rebalance handlers are needed for the initial configuration of the consumer
  // If they are not set, offset reset is ignored and polling produces messages
  // Registering a rebalance_cb turns off librdkafka's automatic partition assignment/revocation and instead delegates that responsibility to the application's rebalance_cb.
  rd_kafka_conf_set_rebalance_cb(conf_.get(), rebalance_cb);

  // setKafkaConfigurationField(conf_.get(), "debug", "all");
  setKafkaConfigurationField(conf_.get(), "bootstrap.servers", utils::getRequiredPropertyOrThrow(context, KafkaBrokers.getName()));  // TODO(hunyadi): replace this with kafka_brokers_
  setKafkaConfigurationField(conf_.get(), "auto.offset.reset", "latest");
  setKafkaConfigurationField(conf_.get(), "enable.auto.commit", "false");
  setKafkaConfigurationField(conf_.get(), "enable.auto.offset.store", "false");
  setKafkaConfigurationField(conf_.get(), "isolation.level", honor_transactions_ ? "read_committed" : "read_uncommitted");
  setKafkaConfigurationField(conf_.get(), "group.id", group_id_);
  setKafkaConfigurationField(conf_.get(), "compression.codec", "snappy");  // FIXME(hunyadi): this seems like a producer property
  setKafkaConfigurationField(conf_.get(), "batch.num.messages", std::to_string(max_poll_records_.value_or(10000)));  // FIXME(hunyadi): this has a default value, maybe not optional(?) // FIXME(hunyadi): this seems like a producer property // NOLINT

  std::array<char, 512U> errstr{};
  consumer_ = { rd_kafka_new(RD_KAFKA_CONSUMER, conf_.release(), errstr.data(), errstr.size()), utils::rd_kafka_consumer_deleter() };
  if (consumer_ == nullptr) {
    const std::string error_msg { errstr.begin(), errstr.end() };
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Failed to create Kafka consumer %s" + error_msg);
  }

  createTopicPartitionList();

  rd_kafka_resp_err_t poll_set_consumer_response = rd_kafka_poll_set_consumer(consumer_.get());
  if (poll_set_consumer_response != RD_KAFKA_RESP_ERR_NO_ERROR) {
    logger_->log_error("\u001b[31mrd_kafka_poll_set_consumer error %d: %s\u001b[0m", poll_set_consumer_response, rd_kafka_err2str(poll_set_consumer_response));
  }

  logger_->log_info("Resetting offset manually.");
  while (true) {
    std::unique_ptr<rd_kafka_message_t, utils::rd_kafka_message_deleter>
        message_wrapper{ rd_kafka_consumer_poll(consumer_.get(), communications_timeout_milliseconds_.count()), utils::rd_kafka_message_deleter() };

    if (!message_wrapper) {
      break;
    }
    utils::print_kafka_message(message_wrapper.get(), logger_);
    // Commit offsets on broker for the provided list of partitions
    const int async = 0;
    logger_->log_info("\u001b[33mCommitting offset: %" PRId64 ".\u001b[0m", message_wrapper->offset);
    rd_kafka_commit_message(consumer_.get(), message_wrapper.get(), async);  // TODO(hunyadi): check this for potential rollback requirements
  }
  logger_->log_info("Done resetting offset manually.");
}

std::string ConsumeKafka::extract_message(const rd_kafka_message_t* rkmessage) {
  if (rkmessage->err != RD_KAFKA_RESP_ERR_NO_ERROR) {
      throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "ConsumeKafka: received error message from broker.");
  }
  utils::print_kafka_message(rkmessage, logger_);
  return { reinterpret_cast<char*>(rkmessage->payload), rkmessage->len };
}

utils::KafkaEncoding ConsumeKafka::key_attr_encoding_attr_to_enum() {
  if (utils::StringUtils::equalsIgnoreCase(key_attribute_encoding_, KEY_ATTR_ENCODING_UTF_8)) {
    return utils::KafkaEncoding::UTF8;
  }
  if (utils::StringUtils::equalsIgnoreCase(key_attribute_encoding_, KEY_ATTR_ENCODING_HEX)) {
    return utils::KafkaEncoding::HEX;
  }
  throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "\"Key Attribute Encoding\" property not recognized.");
}

utils::KafkaEncoding ConsumeKafka::message_header_encoding_attr_to_enum() {
  if (utils::StringUtils::equalsIgnoreCase(message_header_encoding_, MSG_HEADER_ENCODING_UTF_8)) {
    return utils::KafkaEncoding::UTF8;
  }
  if (utils::StringUtils::equalsIgnoreCase(message_header_encoding_, MSG_HEADER_ENCODING_HEX)) {
    return utils::KafkaEncoding::HEX;
  }
  throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "Key Attribute Encoding property not recognized.");
}

std::string ConsumeKafka::resolve_duplicate_headers(const std::vector<std::string>& matching_headers) {
  if(MSG_HEADER_KEEP_FIRST == duplicate_header_handling_) {
    return matching_headers.front();
  }
  if(MSG_HEADER_KEEP_LATEST == duplicate_header_handling_) {
    return matching_headers.back();
  }
  if(MSG_HEADER_COMMA_SEPARATED_MERGE == duplicate_header_handling_) {
    return utils::StringUtils::join(", ", matching_headers);
  }
  throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "\"Duplicate Header Handling\" property not recognized.");
}

std::vector<std::string> ConsumeKafka::get_matching_headers(const rd_kafka_message_t* message, const std::string& header_name) {
  // Headers fetched this way are freed when rd_kafka_message_destroy is called
  // Detaching them using rd_kafka_message_detach_headers does not seem to work
  rd_kafka_headers_t* headers_raw;
  if (RD_KAFKA_RESP_ERR_NO_ERROR != rd_kafka_message_headers(message, &headers_raw)) {
    logger_->log_error("Failed to fetch message headers: %d: %s", rd_kafka_last_error(), rd_kafka_err2str(rd_kafka_last_error()));
  }
  std::vector<std::string> matching_headers;
  for(std::size_t header_idx = 0;; ++header_idx)
  {
    const char* value;  // Not to be freed
    std::size_t size;
    if(RD_KAFKA_RESP_ERR_NO_ERROR != rd_kafka_header_get(headers_raw, header_idx, header_name.c_str(), (const void**)(&value), &size)) {
      break;
    }
    if(size < std::numeric_limits<int>::max()) {
      logger_->log_debug("%.*s", static_cast<int>(size), value);
    }
    matching_headers.emplace_back(value, size);
  }
  return matching_headers;
}

class WriteCallback : public OutputStreamCallback {
 public:
  WriteCallback(char *data, uint64_t size) :
      data_(reinterpret_cast<uint8_t*>(data)),
      dataSize_(size) {}
  uint8_t* data_;
  uint64_t dataSize_;
  int64_t process(const std::shared_ptr<io::BaseStream>& stream) {
    int64_t ret = 0;
    if (data_ && dataSize_ > 0)
      ret = stream->write(data_, dataSize_);
    return ret;
  }
};

void ConsumeKafka::onTrigger(core::ProcessContext* context, core::ProcessSession* session) {
  logger_->log_debug("ConsumeKafka onTrigger");

  print_topics_list(logger_, kf_topic_partition_list_.get());

  std::unique_ptr<rd_kafka_message_t, utils::rd_kafka_message_deleter> message{
      rd_kafka_consumer_poll(consumer_.get(), communications_timeout_milliseconds_.count()), utils::rd_kafka_message_deleter()};
  if (!message) {
    return;
  }

  std::string message_content = extract_message(message.get());
  if (message_content.empty()) {
    return;
  }

  // Commit offsets on broker for the provided list of partitions
  const int async = 0;
  rd_kafka_commit_message(consumer_.get(), message.get(), async);  // TODO(hunyadi): check this for potential rollback requirements
  logger_->log_info("\u001b[33mCommitting offset: %" PRId64 ".\u001b[0m", message->offset);

  std::shared_ptr<FlowFileRecord> flow_file = std::static_pointer_cast<FlowFileRecord>(session->create());
  if (flow_file == nullptr) {
    return;
  }

  WriteCallback callback(&message_content[0], message_content.size());
  session->write(flow_file, &callback);

  for (const std::string& header_name : headers_to_add_as_attributes_) {
    const auto matching_headers = get_matching_headers(message.get(), header_name);
    if(matching_headers.size()) {
      flow_file->setAttribute(header_name, utils::get_encoded_string(resolve_duplicate_headers(matching_headers), message_header_encoding_attr_to_enum()));
    }
  }

  const utils::optional<std::string> message_key = utils::get_encoded_message_key(message.get(), key_attr_encoding_attr_to_enum());
  if (message_key) {
    flow_file->setAttribute(KAFKA_MESSAGE_KEY_ATTR, message_key.value());
  }

  session->transfer(flow_file, Success);
}

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
