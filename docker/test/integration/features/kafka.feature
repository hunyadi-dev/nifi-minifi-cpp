Feature: Sending data to using Kafka streaming platform using PublishKafka
  In order to send data to a Kafka stream
  As a user of MiNiFi
  I need to have PublishKafka processor

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: A MiNiFi instance transfers data to a kafka broker
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a PublishKafka processor set up to communicate with a kafka broker instance
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PublishKafka
    And the "success" relationship of the PublishKafka processor is connected to the PutFile

    And a kafka broker "broker" is set up in correspondence with the PublishKafka

    When both instances start up
    Then a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds

  Scenario: PublishKafka sends flowfiles to failure when the broker is not available
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "no broker" is present in "/tmp/input"
    And a PublishKafka processor set up to communicate with a kafka broker instance
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PublishKafka
    And the "failure" relationship of the PublishKafka processor is connected to the PutFile

    When the MiNiFi instance starts up
    Then a flowfile with the content "no broker" is placed in the monitored directory in less than 60 seconds

  Scenario: PublishKafka sends can use SSL connect
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a PublishKafka processor set up to communicate with a kafka broker instance
    And these processor properties are set:
      | processor name | property name          | property value                             |
      | PublishKafka   | Client Name            | LMN                                        |
      | PublishKafka   | Known Brokers          | kafka-broker:9093                          |
      | PublishKafka   | Topic Name             | test                                       |
      | PublishKafka   | Batch Size             | 10                                         |
      | PublishKafka   | Compress Codec         | none                                       |
      | PublishKafka   | Delivery Guarantee     | 1                                          |
      | PublishKafka   | Request Timeout        | 10 sec                                     |
      | PublishKafka   | Message Timeout Phrase | 12 sec                                     |
      | PublishKafka   | Security CA Key        | /tmp/resources/certs/ca-cert               |
      | PublishKafka   | Security Cert          | /tmp/resources/certs/client_LMN_client.pem |
      | PublishKafka   | Security Pass Phrase   | abcdefgh                                   |
      | PublishKafka   | Security Private Key   | /tmp/resources/certs/client_LMN_client.key |
      | PublishKafka   | Security Protocol      | ssl                                        |
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PublishKafka
    And the "success" relationship of the GetFile processor is connected to the PutFile

    And a kafka broker "broker" is set up in correspondence with the PublishKafka

    When both instances start up
    Then a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds

  Scenario: A MiNiFi publishes data to a kafka topic and another one listens using ConsumeKafka
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a PublishKafka processor set up to communicate with a kafka broker instance

    And the "success" relationship of the GetFile processor is connected to the PublishKafka

    And a ConsumeKafka processor set up to consume from the topic PublishKafka sends data to in a "kafka-consumer-flow" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile

    And a kafka broker "broker" is set up in correspondence with the PublishKafka

    When all instances start up
    Then a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds

  Scenario Outline: ConsumeKafka parses and uses kafka topics
    Given a kafka producer workflow publishing files placed in "/tmp/input" to a broker
    And two files with content "<message 1>" and "<message 2>" are placed in "/tmp/input" 

    And a kafka broker "broker" is set up in correspondence with the publisher flow

    And a ConsumeKafka processor set up to consume from the topic PublishKafka sends data to in a "kafka-consume" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile
    And the "Topic Names" of the ConsumeKafka processor is set to "<topic names>"
    And the "Topic Name Format" of the ConsumeKafka processor is set to "<topic name format>"

    When all instances start up
    Then a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds

  Examples: Topic names and formats to test
    | message 1            | message 2           | topic names              | topic name format |
    | The Great Gatsby     | F. Scott Fitzgerald | ConsumeKafkaTest         | Names             |
    | War and Peace        | Lev Tolstoy         | a,b,c,ConsumeKafkaTest,d | Names             |
    | Nineteen Eighty Four | George Orwell       | ConsumeKafkaTest         | Patterns          |
    | Hamlet               | William Shakespeare | Cons[emu]*KafkaTest      | Patterns          |
