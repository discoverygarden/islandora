<?php

namespace Drupal\islandora\Access;

use Drupal\Core\Routing\Access\AccessInterface;
use Drupal\Core\Session\AccountInterface;
use Drupal\Core\Access\AccessResult;
use Drupal\Core\Config\ConfigFactoryInterface;

/**
 * Access checking for objects within Islandora.
 */
class IslandoraObjectAccess implements AccessInterface {
  /**
   * The config factory.
   *
   * @var \Drupal\Core\Config\ConfigFactoryInterface
   */
  protected $configFactory;

  /**
   * Constructor.
   */
  public function __construct(ConfigFactoryInterface $configFactory) {
    $this->configFactory = $configFactory;
  }

  /**
   * Whether the user has access to an object.
   *
   * @param string|array $perms
   *   A singular permission or an array of permissions to be evalulated.
   * @param string|AbstractObject $object
   *   A string of the default 'root' is being based through, a loaded Fedora
   *   object otherwise.
   * @param \Drupal\Core\Session\AccountInterface $account
   *   User being validated against.
   * @param string $islandora_access_conjunction
   *   If an array of permissions is specified this will dictate how it's
   *   evaluated. To maintain 7's behavior these are ORed together by default
   *   but can be overridden on a per route basis.
   *
   * @return \Drupal\Core\Access\AccessResult|\Drupal\Core\Access\AccessResultAllowed|\Drupal\Core\Access\AccessResultForbidden|\Drupal\Core\Access\AccessResultNeutral
   *   Whether the user has access in AccessResult object form.
   */
  public function access($perms, $object, AccountInterface $account, $islandora_access_conjunction = 'OR') {
    module_load_include('inc', 'islandora', 'includes/utilities');
    // XXX: This seems so very dumb but given how empty slugs don't play nice
    // in Drupal as defaults this needs to be the case. If it's possible to get
    // around this by making the empty slug route in YAML or a custom Routing
    // object we can remove this.
    $object = islandora_object_load($object === 'root' ?
      $this->configFactory->get('islandora.settings')->get('islandora_repository_pid') :
      $object);
    if (!$object && !islandora_describe_repository()) {
      islandora_display_repository_inaccessible_message();
      return AccessResult::forbidden();
    }
    if (is_array($perms)) {
      $result = AccessResult::neutral();
      foreach ($perms as $perm) {
        $result = $islandora_access_conjunction == 'AND' ? $result->andIf(AccessResult::allowedIf(islandora_object_access($perm, $object, $account))) : $result->orIf(AccessResult::allowedIf(islandora_object_access($perm, $object, $account)));
      }
      return $result;
    }
    else {
      return AccessResult::allowedIf(islandora_object_access($perms, $object, $account));
    }
  }

}
