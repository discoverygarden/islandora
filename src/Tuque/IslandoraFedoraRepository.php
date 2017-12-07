<?php

namespace Drupal\islandora\Tuque;

require_once __DIR__ . '/Base.php';

use FedoraRepository;
use NewFedoraObject;

/**
 * Class IslandoraFedoraRepository.
 *
 * @package Drupal\islandora\Tuque
 */
class IslandoraFedoraRepository extends FedoraRepository {
  protected $queryClass = IslandoraRepositoryQuery::class;
  protected $newObjectClass = IslandoraNewFedoraObject::class;
  protected $objectClass = IslandoraFedoraObject::class;

  /**
   * Ingest the given object.
   *
   * @see FedoraRepository::ingestObject()
   */
  public function ingestObject(NewFedoraObject &$object) {
    try {
      foreach ($object as $dsid => $datastream) {
        $datastream_context = [
          'action' => 'ingest',
          'block' => FALSE,
        ];
        islandora_alter_datastream($object, $datastream, $datastream_context);
        if ($datastream_context['block']) {
          throw new Exception(t('Object ingest blocked due to ingest of @dsid being blocked.', [
            '@dsid' => $dsid,
          ]));
        }
      }

      $object_context = [
        'action' => 'ingest',
        'block' => FALSE,
      ];
      islandora_alter_object($object, $object_context);
      if ($object_context['block']) {
        throw new Exception('Ingest Object was blocked.');
      }
      $ret = parent::ingestObject($object);
      islandora_invoke_object_hooks(ISLANDORA_OBJECT_INGESTED_HOOK, $object->models, $object);
      // Call the ingested datastream hooks for NewFedoraObject's after the
      // object had been ingested.
      foreach ($object as $dsid => $datastream) {
        islandora_invoke_datastream_hooks(ISLANDORA_DATASTREAM_INGESTED_HOOK, $object->models, $dsid, $object, $datastream);
      }
      return $ret;
    }
    catch (Exception $e) {
      \Drupal::logger('islandora')->error(
        'Failed to ingest object: @pid</br>code: @code<br/>message: @msg', [
          '@pid' => $object->id,
          '@code' => $e->getCode(),
          '@msg' => $e->getMessage(),
        ]
      );
      throw $e;
    }
  }

  /**
   * Constructs a Fedora Object.
   *
   * @see FedoraRepository::constructObject
   */
  public function constructObject($id = NULL, $create_uuid = NULL) {
    // Enforces UUID when set, but allows to override if called
    // with $create_uuid set to bool.
    return parent::constructObject($id, static::useUUIDs($create_uuid));
  }

  /**
   * Get the next PID(s) from Repo.
   *
   * @see FedoraRepository::getNextIdentifier()
   */
  public function getNextIdentifier($namespace = NULL, $create_uuid = NULL, $count = 1) {
    // Enforces UUID when set, but allows to override if called
    // with $create_uuid set to bool.
    return parent::getNextIdentifier($namespace, static::useUUIDs($create_uuid), $count);
  }

  /**
   * Helper for three-valued logic with UUIDs.
   *
   * @param bool|null $to_create
   *   The variable to test.
   *
   * @return bool
   *   If $to_create is NULL, the value of the
   *   'islandora_basic_collection_generate_uuid' Drupal variable; otherwise,
   *   the value of $to_create itself.
   */
  // @codingStandardsIgnoreStart
  protected static function useUUIDs($to_create) {
  // Rename would break older functionality.   
  // @codingStandardsIgnoreEnd
    return is_null($to_create) ?
      \Drupal::config('islandora.settings')->get('islandora_basic_collection_generate_uuid') :
      $to_create;
  }

}
