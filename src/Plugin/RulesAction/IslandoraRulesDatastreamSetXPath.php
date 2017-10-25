<?php

namespace Drupal\islandora\Plugin\RulesAction;

use Drupal\rules\Core\RulesActionBase;
use DOMXPath;
use Drupal\islandora\TypedData\IslandoraXPathTrait;

/**
 * Rules action; Run a query against the given DOMXPath instance.
 *
 * @RulesAction(
 *   id = "islandora_rules_datastream_query_xpath",
 *   label = @Translation("Query nodes from DOMXPath instance."),
 *   category = @Translation("Islandora DOMXPath"),
 *   context = {
 *     "object" = @ContextDefinition("islandora_object",
 *       label = @Translation("Subject"),
 *       description = @Translation("The object to check for the datastream.")),
 *     "datastream_id" = @ContextDefinition("string",
 *       label = @Translation("Datastream ID",
 *       description = @Translation("A string containing the identity of the datastream to look for on the object."))),
 *     "xpath" = @ContextDefinition("string",
 *       label = @Translation("XPath"),
 *       description = @Translation("The XPath to test.")),
 *     "xpath_namespaces" = @ContextDefinition("taxonomy_vocabulary",
 *       label = @Translation("XPath Namespace Taxonomy"),
 *       description = @Translation("A flat taxonomy of which the terms are namespace prefixes and the description contains the URI for the namespace."),
 *       required = false),
 *     "value" = @ContextDefinition("string",
 *       label = @Translation("Value"),
 *       description = @Translation("The value to set in the XML on elements matched by the XPath.")),
 *   }
 * )
 */
class IslandoraRulesDatastreamSetXPath extends RulesActionBase {
  use IslandoraXPathTrait;

  /**
   * {@inheritdoc}
   */
  protected function doExecute($object, $datastream_id, $xpath, $xpath_namespaces, $value) {
    $dom_xpath = $this->loadXpathFromObject($object, $datastream_id, $xpath_namespaces);
    $results = $dom_xpath->query($xpath);
    foreach ($results as $result) {
      $result->nodeValue = $value;
    }
  }

}
